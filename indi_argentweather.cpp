/*******************************************************************************
  Copyright(c) 2024 Gord Tulloch. All rights reserved.

  INDI Driver for the Argent Data Systems ADS-WS1 Weather Station

  Serial packet parsing derived from:
    https://github.com/EvanVS/WXparser  (EvanVS)
    https://github.com/gordtulloch/MCP  (Gord Tulloch)

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "indi_argentweather.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>

// !! + 12 × 4 hex chars + \r\n = 52 bytes
#define ARGENT_PACKET_LEN  52
#define ARGENT_BUF         64

// Seconds to wait for one packet when reading
#define ARGENT_READ_TIMEOUT  5

// A representative simulation packet
// Wind=15 kph, Dir=S(180°), OutTemp=15°C(59°F), TotalRain=0,
// Baro=1013 mbar, IndoorTemp=20°C(68°F), OutHum=60%, IndoorHum=50%,
// Day=1, Min=720, TodayRain=0, AvgWind=15 kph
#define ARGENT_SIM_PACKET "!!00960080024E0000279202A8025801F4000102D000000096\r\n"

static std::unique_ptr<ArgentWeather> argentWeather(new ArgentWeather());

/******************************************************************************
 * Constructor
 ******************************************************************************/
ArgentWeather::ArgentWeather()
{
    setVersion(1, 0);
}

/******************************************************************************
 * getDefaultName
 ******************************************************************************/
const char *ArgentWeather::getDefaultName()
{
    return "Argent ADS-WS1";
}

/******************************************************************************
 * initProperties
 ******************************************************************************/
bool ArgentWeather::initProperties()
{
    INDI::Weather::initProperties();

    // ---- Weather safety parameters ----
    // Temperature: safe between -10°C and 35°C
    addParameter("WEATHER_TEMPERATURE", "Outdoor Temperature (°C)", -10, 35, 15);
    // Humidity: safe below 90%
    addParameter("WEATHER_HUMIDITY",    "Outdoor Humidity (%)",       0, 90, 15);
    // Barometric pressure: nominal range
    addParameter("WEATHER_BAROMETER",   "Barometer (mbar)",         980, 1040, 15);
    // Wind speed: safe below 50 km/h
    addParameter("WEATHER_WIND_SPEED",  "Wind Speed (km/h)",          0,  50, 15);
    // 1-minute average wind speed: safe below 50 km/h
    addParameter("WEATHER_WIND_GUST",   "Wind 1-min Avg (km/h)",      0,  50, 15);
    // Dew point: informational; wide range to avoid false alarms
    addParameter("WEATHER_DEWPOINT",    "Dew Point (°C)",            -20,  30, 15);
    // Today's rain total: minOK=-1 and maxOK=0 so any rain (>0 mm) is unsafe
    addParameter("WEATHER_RAIN_HOUR",   "Rain Today (mm)",            -1,   0, 15);

    // Wind speed and rain are critical: if out of range the observatory is unsafe
    setCriticalParameter("WEATHER_WIND_SPEED");
    setCriticalParameter("WEATHER_WIND_GUST");
    setCriticalParameter("WEATHER_RAIN_HOUR");

    // ---- Display-only properties (not used for safety decisions) ----

    IndoorNP[0].fill("INDOOR_TEMPERATURE", "Temperature (°C)", "%.1f", -50, 80, 0, 0);
    IndoorNP[1].fill("INDOOR_HUMIDITY",    "Humidity (%)",     "%.1f",   0, 100, 0, 0);
    IndoorNP.fill(getDeviceName(), "ARGENT_INDOOR", "Indoor",
                  MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    WindDirNP[0].fill("WIND_BEARING", "Bearing (°)", "%.0f", 0, 360, 0, 0);
    WindDirNP.fill(getDeviceName(), "ARGENT_WIND_DIRECTION", "Wind Direction",
                   MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    WindDirTP[0].fill("WIND_HEADING", "Heading", "--");
    WindDirTP.fill(getDeviceName(), "ARGENT_WIND_HEADING", "Wind Heading",
                   MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    RainNP[0].fill("TODAY_RAIN_MM",  "Today (mm)",       "%.2f", 0, 9999, 0, 0);
    RainNP[1].fill("TOTAL_RAIN_MM",  "Long-term (mm)",   "%.2f", 0, 9999, 0, 0);
    RainNP.fill(getDeviceName(), "ARGENT_RAIN", "Rainfall",
                MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // ADS-WS1 transmits at 2400 baud
    serialConnection->setDefaultBaudRate(Connection::Serial::B_2400);

    addAuxControls();

    return true;
}

/******************************************************************************
 * updateProperties – register / remove UI properties on connect / disconnect
 ******************************************************************************/
bool ArgentWeather::updateProperties()
{
    INDI::Weather::updateProperties();

    if (isConnected())
    {
        defineProperty(IndoorNP);
        defineProperty(WindDirNP);
        defineProperty(WindDirTP);
        defineProperty(RainNP);
    }
    else
    {
        deleteProperty(IndoorNP);
        deleteProperty(WindDirNP);
        deleteProperty(WindDirTP);
        deleteProperty(RainNP);
    }

    return true;
}

/******************************************************************************
 * Handshake – verify the device is present and streaming valid data
 ******************************************************************************/
bool ArgentWeather::Handshake()
{
    if (isSimulation())
    {
        LOG_INFO("Simulation mode – ADS-WS1 connected.");
        return true;
    }

    char buf[ARGENT_BUF] = {0};
    int  nbytes           = 0;
    int  rc;

    // The station streams continuously; try up to 3 reads (each up to
    // ARGENT_READ_TIMEOUT seconds) so we tolerate a partial first line.
    LOG_INFO("Waiting for ADS-WS1 data (may take several seconds)...");

    for (int attempt = 0; attempt < 3; attempt++)
    {
        memset(buf, 0, sizeof(buf));
        rc = tty_read_section(PortFD, buf, '\n', ARGENT_READ_TIMEOUT, &nbytes);

        if (rc == TTY_OK && nbytes >= 2 && buf[0] == '!' && buf[1] == '!')
        {
            LOG_INFO("ADS-WS1 weather station connected and streaming.");
            return true;
        }
    }

    LOG_ERROR("No valid packet received. Check serial port and baud rate (2400 8N1).");
    return false;
}

/******************************************************************************
 * updateWeather – read one packet, parse it, update all INDI properties
 ******************************************************************************/
IPState ArgentWeather::updateWeather()
{
    char buf[ARGENT_BUF] = {0};
    int  nbytes           = 0;

    if (isSimulation())
    {
        strncpy(buf, ARGENT_SIM_PACKET, ARGENT_BUF - 1);
        nbytes = ARGENT_PACKET_LEN;
    }
    else
    {
        int rc = tty_read_section(PortFD, buf, '\n', ARGENT_READ_TIMEOUT, &nbytes);
        if (rc != TTY_OK)
        {
            char errstr[MAXRBUF];
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Read error: %s", errstr);
            return IPS_ALERT;
        }
    }

    LOGF_DEBUG("RX <%s>", buf);

    if (!parsePacket(buf))
        return IPS_ALERT;

    IndoorNP.setState(IPS_OK);
    IndoorNP.apply();

    WindDirNP.setState(IPS_OK);
    WindDirNP.apply();

    WindDirTP.setState(IPS_OK);
    WindDirTP.apply();

    RainNP.setState(IPS_OK);
    RainNP.apply();

    return IPS_OK;
}

/******************************************************************************
 * parsePacket – decode a 52-byte ADS-WS1 serial packet
 *
 * Packet layout (ASCII, 52 bytes total):
 *   [0-1]   !!          header
 *   [2-5]   WWWW        current wind speed, 0.1 kph
 *   [6-9]   DDDD        wind direction, 0-255 (maps to 0-360°)
 *   [10-13] TTTT        outdoor temperature, 0.1°F
 *   [14-17] RRRR        long-term rain total, 0.01 inches
 *   [18-21] BBBB        barometric pressure, 0.1 mbar
 *   [22-25] IIII        indoor temperature, 0.1°F
 *   [26-29] HHHH        outdoor relative humidity, 0.1%
 *   [30-33] JJJJ        indoor relative humidity, 0.1%
 *   [34-37] YYYY        day of year (not used)
 *   [38-41] MMMM        minute of day (not used)
 *   [42-45] AAAA        today's rain total, 0.01 inches
 *   [46-49] GGGG        1-minute average wind speed, 0.1 kph
 *   [50-51] \r\n        end of packet
 ******************************************************************************/
bool ArgentWeather::parsePacket(const char *buf)
{
    if (buf[0] != '!' || buf[1] != '!')
    {
        LOGF_ERROR("Unexpected header bytes: 0x%02X 0x%02X",
                   (unsigned char)buf[0], (unsigned char)buf[1]);
        return false;
    }

    // Parse a 4-character hex field starting at buf[offset]
    char hex[5];
    auto field = [&](int offset) -> int {
        memcpy(hex, buf + offset, 4);
        hex[4] = '\0';
        return static_cast<int>(strtol(hex, nullptr, 16));
    };

    // Raw hex values
    int windSpeedRaw   = field(2);   // 0.1 kph
    int windDirRaw     = field(6);   // 0-255
    int outdoorTempRaw = field(10);  // 0.1°F
    int totalRainRaw   = field(14);  // 0.01 in
    int baromRaw       = field(18);  // 0.1 mbar
    int indoorTempRaw  = field(22);  // 0.1°F
    int outdoorHumRaw  = field(26);  // 0.1%
    int indoorHumRaw   = field(30);  // 0.1%
    int todayRainRaw   = field(42);  // 0.01 in
    int avgWindRaw     = field(46);  // 0.1 kph

    // ---- Engineering-unit conversions ----

    double windSpeedKph  = windSpeedRaw  / 10.0;
    double avgWindKph    = avgWindRaw    / 10.0;
    double windBearing   = (windDirRaw  / 255.0) * 360.0;

    double outdoorTempF  = outdoorTempRaw / 10.0;
    double outdoorTempC  = (outdoorTempF - 32.0) / 1.8;

    double indoorTempF   = indoorTempRaw  / 10.0;
    double indoorTempC   = (indoorTempF  - 32.0) / 1.8;

    double pressureMbar  = baromRaw      / 10.0;

    double outdoorHum    = outdoorHumRaw / 10.0;  // %
    double indoorHum     = indoorHumRaw  / 10.0;  // %

    // Convert inches to millimetres (1 in = 25.4 mm)
    double totalRainMm   = (totalRainRaw  / 100.0) * 25.4;
    double todayRainMm   = (todayRainRaw  / 100.0) * 25.4;

    double dewpointC     = calcDewpoint(outdoorTempC, outdoorHum);

    // ---- Compass heading ----
    static const char *compass[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    int compassIdx = static_cast<int>(std::round(windBearing / 45.0)) % 8;
    WindDirTP[0].setText(compass[compassIdx]);

    // ---- Update weather safety parameters ----
    setParameterValue("WEATHER_TEMPERATURE", outdoorTempC);
    setParameterValue("WEATHER_HUMIDITY",    outdoorHum);
    setParameterValue("WEATHER_BAROMETER",   pressureMbar);
    setParameterValue("WEATHER_WIND_SPEED",  windSpeedKph);
    setParameterValue("WEATHER_WIND_GUST",   avgWindKph);
    setParameterValue("WEATHER_DEWPOINT",    dewpointC);
    setParameterValue("WEATHER_RAIN_HOUR",   todayRainMm);

    // ---- Update display-only properties ----
    IndoorNP[0].setValue(indoorTempC);
    IndoorNP[1].setValue(indoorHum);

    WindDirNP[0].setValue(windBearing);

    RainNP[0].setValue(todayRainMm);
    RainNP[1].setValue(totalRainMm);

    LOGF_INFO("T=%.1f°C H=%.0f%% P=%.1fmbar W=%.1fkm/h Avg=%.1fkm/h Dir=%.0f°%s Rain=%.2fmm",
              outdoorTempC, outdoorHum, pressureMbar,
              windSpeedKph, avgWindKph,
              windBearing, compass[compassIdx],
              todayRainMm);

    return true;
}

/******************************************************************************
 * calcDewpoint – Magnus approximation
 *
 * Returns dew-point temperature in °C.
 ******************************************************************************/
double ArgentWeather::calcDewpoint(double tempC, double humidity)
{
    if (humidity <= 0.0)
        return tempC;  // fallback: dew point cannot exceed air temperature

    const double a = 17.271;
    const double b = 237.7;
    double g = (a * tempC / (b + tempC)) + std::log(humidity / 100.0);
    return (b * g) / (a - g);
}
