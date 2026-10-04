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

#pragma once

#include "indiweather.h"

/**
 * @brief The ArgentWeather class provides an INDI weather driver for the
 * Argent Data Systems ADS-WS1 weather station.
 *
 * The ADS-WS1 continuously streams 52-byte ASCII packets over a serial port
 * at 2400 baud.  Each packet has the form:
 *
 *   !! WWWW DDDD TTTT RRRR BBBB IIII HHHH JJJJ YYYY MMMM AAAA GGGG \r\n
 *
 * where every field is a 4-character uppercase hex string and represents:
 *   WWWW – current wind speed in 0.1 kph
 *   DDDD – wind direction 0-255 (maps to 0-360°)
 *   TTTT – outdoor temperature in 0.1°F
 *   RRRR – long-term rain total in 0.01 inches
 *   BBBB – barometric pressure in 0.1 mbar
 *   IIII – indoor temperature in 0.1°F
 *   HHHH – outdoor relative humidity in 0.1%
 *   JJJJ – indoor relative humidity in 0.1%
 *   YYYY – day of year (informational)
 *   MMMM – minute of day (informational)
 *   AAAA – today's rain total in 0.01 inches
 *   GGGG – 1-minute average wind speed in 0.1 kph
 */
class ArgentWeather : public INDI::Weather
{
public:
    ArgentWeather();

    virtual bool        Handshake()        override;
    virtual const char *getDefaultName()   override;
    virtual bool        initProperties()   override;
    virtual bool        updateProperties() override;

protected:
    virtual IPState updateWeather() override;

private:
    bool   parsePacket(const char *buf);
    double calcDewpoint(double tempC, double humidity);

    // Indoor temperature (°C) and relative humidity (%)
    INDI::PropertyNumber IndoorNP {2};

    // Wind direction: bearing in degrees and compass heading string
    INDI::PropertyNumber WindDirNP {1};
    INDI::PropertyText   WindDirTP {1};

    // Rain accumulation: today's total (mm) and long-term total (mm)
    INDI::PropertyNumber RainNP {2};
};
