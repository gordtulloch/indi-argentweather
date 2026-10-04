# INDI Driver for Argent Data Systems ADS-WS1 Weather Station

An [INDI](https://indilib.org) 2.x weather driver for the **Argent Data Systems ADS-WS1** personal weather station. It reads the continuous serial data stream from the station, decodes all sensor fields, exposes them as INDI properties, and participates in INDI's weather-safety system so that observatory automation clients (KStars/Ekos, Voyager, etc.) can gate dome or mount operations on live weather conditions.

---

## Table of Contents

- [Hardware](#hardware)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Building](#building)
- [Installation](#installation)
- [Configuration](#configuration)
  - [Selecting the Serial Port](#selecting-the-serial-port)
  - [Tuning Safety Thresholds](#tuning-safety-thresholds)
- [INDI Properties Reference](#indi-properties-reference)
  - [Weather Safety Parameters](#weather-safety-parameters)
  - [Display-Only Properties](#display-only-properties)
- [Serial Protocol](#serial-protocol)
- [Simulation Mode](#simulation-mode)
- [Troubleshooting](#troubleshooting)
- [Credits](#credits)
- [License](#license)

---

## Hardware

| Item | Detail |
|------|--------|
| Device | Argent Data Systems ADS-WS1 weather station |
| Interface | RS-232 serial (DB9 or USB-to-serial adapter) |
| Baud rate | **2400 baud, 8N1** |
| Data format | Continuous ASCII stream, one 52-byte packet per update cycle |

The ADS-WS1 measures outdoor temperature, outdoor humidity, barometric pressure, wind speed, wind direction, and rainfall. It also reports indoor temperature and humidity.

---

## Features

- Full INDI 2.2.5 `INDI::Weather` integration with serial connection plugin
- User-selectable serial port from the INDI client UI — no recompilation needed
- All sensor fields decoded and exposed as INDI properties:
  - Outdoor temperature, humidity, dew point, barometric pressure
  - Wind speed (instantaneous + 1-minute average), wind bearing (°) and compass heading
  - Today's and long-term cumulative rainfall
  - Indoor temperature and humidity
- Configurable per-parameter warning/error thresholds for observatory safety
- Three parameters marked **critical** (wind speed, wind 1-min average, and today's rain): any out-of-range reading drives the INDI weather status to `UNSAFE`
- Simulation mode for testing without physical hardware
- Dew-point calculation using the Magnus approximation

---

## Prerequisites

| Requirement | Minimum version |
|-------------|-----------------|
| INDI library | 2.0 (tested with 2.2.5) |
| CMake | 3.16 |
| C++ compiler | C++17 (GCC 9+ / Clang 10+) |

### Installing INDI on Debian / Ubuntu / Raspberry Pi OS

```bash
sudo apt-get update
sudo apt-get install libindi-dev indi-bin cmake build-essential
```

### Installing INDI on Fedora / CentOS Stream

```bash
sudo dnf install indi-devel cmake gcc-c++
```

---

## Building

```bash
git clone https://github.com/gordtulloch/indi-argentweather.git
cd indi-argentweather
mkdir build && cd build
cmake ..
make -j$(nproc)
```

---

## Installation

```bash
sudo make install
```

This installs two files:

| File | Destination |
|------|-------------|
| `indi_argentweather` | `/usr/bin/` (or `/usr/local/bin/`) |
| `indi_argentweather.xml` | INDI data directory (typically `/usr/share/indi/`) |

After installation the driver appears in KStars under **Ekos → Profile Editor → Weather Stations → Argent ADS-WS1**.

---

## Configuration

### Selecting the Serial Port

The serial port is configured through the INDI client UI — no file editing is required.

1. In **KStars / Ekos**, open the **Profile Editor** and add *Argent ADS-WS1* to your equipment profile.
2. Connect to the driver.
3. In the driver's **Connection** tab, set **Serial Port** to the device node for your station, e.g.:
   - Linux USB-to-serial: `/dev/ttyUSB0` or `/dev/ttyUSB1`
   - Linux native serial: `/dev/ttyS0`
4. The baud rate is pre-set to **2400**; leave it unchanged.
5. Click **Connect**.

The driver will wait up to 15 seconds for the first valid packet before declaring a connection failure. If you are unsure which port the station is on, run:

```bash
ls -l /dev/ttyUSB* /dev/ttyS*
dmesg | grep tty
```

> **Tip:** On most Linux systems, your user must be in the `dialout` group to access serial ports without root privileges:
> ```bash
> sudo usermod -aG dialout $USER
> # log out and back in for the change to take effect
> ```

### Tuning Safety Thresholds

All weather-safety thresholds are fully configurable from the INDI client. In the driver's **Weather** tab, each parameter shows:

| Column | Meaning |
|--------|---------|
| **Min OK** | Values below this are flagged as warnings or errors |
| **Max OK** | Values above this are flagged as warnings or errors |
| **% Warn** | How close to the limit before a warning is issued |

The factory defaults are conservative starting points; adjust them to suit your location and equipment:

| Parameter | Default Min OK | Default Max OK | Notes |
|-----------|---------------|----------------|-------|
| Outdoor Temperature (°C) | −10 | 35 | Protects optics and mounts |
| Outdoor Humidity (%) | 0 | 90 | Risk of dew / condensation |
| Barometer (mbar) | 980 | 1040 | Informational — not critical |
| Wind Speed (km/h) | 0 | 50 | **Critical** |
| Wind 1-min Avg (km/h) | 0 | 50 | **Critical** — sustained gusts |
| Dew Point (°C) | −20 | 30 | Informational |
| Rain Today (mm) | −1 | 0 | **Critical** — any rain ≥ 0.01 mm is unsafe |

Parameters flagged **Critical** drive the overall INDI weather state to `UNSAFE` immediately when they exceed their threshold. Non-critical parameters produce a `WARNING` state.

---

## INDI Properties Reference

### Weather Safety Parameters

These are standard INDI weather parameters used by clients for safety decisions.

| Property name | Label | Unit | Critical |
|---------------|-------|------|----------|
| `WEATHER_TEMPERATURE` | Outdoor Temperature | °C | No |
| `WEATHER_HUMIDITY` | Outdoor Humidity | % | No |
| `WEATHER_BAROMETER` | Barometer | mbar | No |
| `WEATHER_WIND_SPEED` | Wind Speed | km/h | **Yes** |
| `WEATHER_WIND_GUST` | Wind 1-min Avg | km/h | **Yes** |
| `WEATHER_DEWPOINT` | Dew Point | °C | No |
| `WEATHER_RAIN_HOUR` | Rain Today | mm | **Yes** |

### Display-Only Properties

These are informational properties not used for safety decisions.

| Property | Elements | Description |
|----------|----------|-------------|
| `ARGENT_INDOOR` | `INDOOR_TEMPERATURE` (°C), `INDOOR_HUMIDITY` (%) | Indoor sensor readings |
| `ARGENT_WIND_DIRECTION` | `WIND_BEARING` (°) | Wind direction in degrees 0–360 |
| `ARGENT_WIND_HEADING` | `WIND_HEADING` | Compass heading string (N, NE, E, SE, S, SW, W, NW) |
| `ARGENT_RAIN` | `TODAY_RAIN_MM` (mm), `TOTAL_RAIN_MM` (mm) | Today's and long-term cumulative rainfall |

---

## Serial Protocol

The ADS-WS1 continuously transmits 52-byte ASCII packets at 2400 baud, 8N1. Each packet has the following layout:

```
!! WWWW DDDD TTTT RRRR BBBB IIII HHHH JJJJ YYYY MMMM AAAA GGGG \r\n
```

Every field between `!!` and `\r\n` is a **4-character uppercase hexadecimal integer**.

| Bytes | Field | Raw unit | Converted |
|-------|-------|----------|-----------|
| 0–1 | `!!` | — | Packet header |
| 2–5 | `WWWW` | 0.1 kph | Wind speed (km/h) |
| 6–9 | `DDDD` | 0–255 | Wind direction → 0–360° via `(raw / 255) × 360` |
| 10–13 | `TTTT` | 0.1 °F | Outdoor temperature → °C |
| 14–17 | `RRRR` | 0.01 in | Long-term rain total → mm |
| 18–21 | `BBBB` | 0.1 mbar | Barometric pressure (mbar) |
| 22–25 | `IIII` | 0.1 °F | Indoor temperature → °C |
| 26–29 | `HHHH` | 0.1 % | Outdoor relative humidity (%) |
| 30–33 | `JJJJ` | 0.1 % | Indoor relative humidity (%) |
| 34–37 | `YYYY` | day | Day of year (informational, not exposed) |
| 38–41 | `MMMM` | minute | Minute of day (informational, not exposed) |
| 42–45 | `AAAA` | 0.01 in | Today's rain total → mm |
| 46–49 | `GGGG` | 0.1 kph | 1-minute average wind speed (km/h) |
| 50–51 | `\r\n` | — | End of packet |

### Unit Conversions Applied by the Driver

| Measurement | Conversion |
|-------------|------------|
| Temperature | `(raw_tenths_°F / 10 − 32) / 1.8` → °C |
| Pressure | `raw_tenths_mbar / 10` → mbar |
| Wind speed | `raw_tenths_kph / 10` → km/h |
| Rainfall | `(raw_hundredths_in / 100) × 25.4` → mm |
| Wind bearing | `(raw_0_255 / 255) × 360` → degrees |
| Dew point | Magnus approximation: `(b × γ) / (a − γ)` where `γ = (aT)/(b+T) + ln(RH/100)`, `a = 17.271`, `b = 237.7` |

---

## Simulation Mode

Enable simulation mode in the INDI client (**Connection tab → Simulation → On**) to run the driver without hardware. In this mode the driver returns a fixed packet representing:

| Field | Simulated value |
|-------|----------------|
| Outdoor temperature | 15.0 °C (59 °F) |
| Outdoor humidity | 60.0 % |
| Barometric pressure | 1013.0 mbar |
| Wind speed | 15.0 km/h |
| Wind 1-min average | 15.0 km/h |
| Wind direction | ~181° (South) |
| Today's rain | 0.00 mm |
| Indoor temperature | 20.0 °C (68 °F) |
| Indoor humidity | 50.0 % |

All thresholds and client integrations can be exercised in simulation mode before connecting real hardware.

---

## Troubleshooting

### Driver fails to connect

- Verify the serial port path in the **Connection** tab.
- Confirm the station is powered and the cable is seated.
- Check baud rate is set to **2400**.
- Ensure your user is in the `dialout` group (see [Selecting the Serial Port](#selecting-the-serial-port)).
- Run `screen /dev/ttyUSB0 2400` (replace port as needed) and confirm you see raw `!!` data scrolling.

### Driver connects but Weather status stays `IDLE`

- The `updateWeather` poll interval defaults to 60 seconds in the INDI weather base class. Trigger an immediate update by clicking **Update** in the driver's Weather tab, or wait for the next poll.

### All readings show 0.0

- The station may have transmitted a partial packet on the first read. The driver will recover on the next `updateWeather` call.
- Enable debug logging in your INDI client and look for `RX <...>` lines to inspect the raw packet.

### Unexpected UNSAFE state

- Check the **Weather** tab for which parameter triggered the unsafe condition.
- Adjust the **Max OK** threshold for that parameter to a value appropriate for your site.

---

## Credits

Serial packet format and parsing logic derived from:

- **WXparser** by [EvanVS](https://github.com/EvanVS/WXparser) — comprehensive ADS-WS1 decoder with full unit conversion and dew-point calculation.
- **MCP** by [Gord Tulloch](https://github.com/gordtulloch/MCP) — observatory management system including a functional ADS-WS1 module used as the basis for this driver.

INDI driver architecture follows patterns established in the [INDI library](https://github.com/indilib/indi) drivers, in particular the MBox and Vantage serial weather station drivers.

---

## License

Copyright © 2024 Gord Tulloch.

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License** as published by the Free Software Foundation, either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but **without any warranty**; without even the implied warranty of merchantability or fitness for a particular purpose. See the [GNU General Public License](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html) for more details.
