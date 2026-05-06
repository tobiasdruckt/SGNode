# ESP32 Fermentation Float Unit - Alpha v0.1.0

## Overview
Battery-powered ESP32 fermentation monitoring float unit that measures tilt via BMI160 IMU and temperature via BMP180, transmitting data via ESP-NOW to base station.

## Features
- ✅ **Sensors**: BMI160 IMU (tilt) + BMP180 (temperature/pressure)
- ✅ **Calculations**: Specific gravity from tilt angle
- ✅ **Wireless**: ESP-NOW transmission to base station
- ✅ **Battery**: 4.164V monitoring with calibrated voltage divider
- ✅ **SOC**: State of charge calculation (100% at 4.175V)
- ✅ **Power**: Deep sleep cycle (180s intervals)
- ✅ **Calibration**: Polynomial calibration system

## Hardware Configuration
- **Board**: ESP32 (WeMos D32 compatible)
- **I2C**: SDA=GPIO26, SCL=GPIO27
- **Battery**: GPIO15 (ADC1_CH3) with 2:1 voltage divider
- **LEDs**: GPIO5 (built-in), GPIO16 (extra)
- **Calibration**: GPIO12 (pull-down switch)

## Battery Monitoring
- **Voltage Divider**: 10kΩ + 10kΩ (2:1 ratio)
- **Calibration Factor**: 0.913 (cumulative)
- **SOC Range**: 3.0V (0%) to 4.175V (100%)
- **Accuracy**: ±0.01V after calibration

## Data Transmission
```
Protocol Version: 2
Payload: SG, Temperature, Battery Voltage, Sequence ID, Flags, CRC
Interval: 180 seconds (configurable 60-600s)
```

## Installation
1. Install required libraries:
   - EmotiBit BMI160 (from GitHub ZIP)
   - Adafruit BMP085 Library
   - Wire, EEPROM (built-in)

2. Configure base station MAC address in code:
   ```cpp
   uint8_t baseStationMac[] = {0xA4, 0xF0, 0x0F, 0x68, 0x22, 0x00};
   ```

3. Upload sketch and connect battery voltage divider to GPIO15

## Calibration
- Physical switch on GPIO12 enables calibration mode
- Polynomial calibration stored in EEPROM
- Use base station commands for calibration points

## Status
**Alpha v0.1.0 - Working Nightly Build**
- ✅ All core functions operational
- ✅ Battery monitoring calibrated
- ✅ ESP-NOW transmission reliable
- ✅ Deep sleep power management
- ✅ Sensor data accurate

## Power Consumption
- **Active**: ~80mA (sensors + WiFi)
- **Deep Sleep**: ~10µA
- **Battery Life**: ~2-3 weeks (typical usage)

## Troubleshooting
- **No data received**: Check base station MAC address
- **Battery reads 0V**: Verify voltage divider connection to GPIO15
- **WiFi issues**: Ensure ADC1 pins used (GPIO15 is ADC1_CH3)

## Development Notes
- Battery voltage read before WiFi initialization to avoid ADC interference
- SOC calculated locally but not transmitted (base station can calculate)
- Calibration factor accounts for voltage divider and ADC tolerance
- Payload size maintained for base station compatibility
