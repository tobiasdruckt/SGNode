# SGNode Project Overview

## What is SGNode?

ESP32-based fermentation monitoring and temperature control for brewing (beer/wine/cider/mead). Self-contained system with no cloud/phone app.

**Status:** alpha 0.5.0 (Float/Base tested; Plug awaiting water/fridge validation)

## Architecture

```
SGNode_Base (ESP32) ──ESP-NOW── SGNode_Float (ESP32)
                      │
                      └──ESP-NOW── SGNode_Plug (ESP8266)
```

- **Base:** 4" touchscreen, SD card, receives float data, displays charts/UI
- **Float:** Battery-powered, BMI160 IMU tilt sensor, deep sleep, ESP-NOW TX
- **Plug:** ESP8285 smart plug, DS18B20 sensors, PI control, relay output

## Core Components

### SGNode_Shared/
- `sg_protocol.h` - ESP-NOW payload format (v2), command/status structures, CRC16
- `plug_hardware.h` - Plug pin assignments, relay control helpers

### SGNode_Base/src/
- `ui/` - Brew Wizard, touchscreen UI components
- `domain/` - Batch model, yeast presets, state machine, recommendations
- `calculations/` - Brix/SG conversion, ETA, OG verification, target curves
- `test/` - Optional UI test harness

### SGNode_Float/
- `polynomial_calibration.h` - SG calibration from tilt (EEPROM-stored)
- Deep sleep between measurements, retry buffer for missed ACKs

### SGNode_Plug/src/
- `plug_sensors.cpp` - DS18B20 temperature reading
- `beer_pi_controller.cpp` - PI control with D-brake, anti-windup
- `air_controller.cpp` - Fridge air target calculation
- `relay_history.cpp` - Duty cycle tracking

## Data Flow

1. Float measures tilt/temp/battery → sends ESP-NOW packet to base
2. Base receives, calculates SG/attenuation, logs to SD (CSV)
3. Plug receives beer target from base → controls relay via PI algorithm
4. User interacts via base touchscreen (Live, Graph, Dashboard, More views)

## Persistence

- **SD card:** `/data/batches/<id>/` with `profile.json`, `target.json`, `log.csv`
- **Yeast:** `yeast/` directory with custom presets, `yeasts.txt` index
- **EEPROM (Float):** Polynomial coefficients, zero-cal offset

## Key Protocols

- **ESP-NOW:** Channel 1, CRC16-CCITT checksums, sequence numbers
- **Calibration:** Polynomial (cubic) SG from tilt, zero-cal via ACK commands
- **Communication:** Base→Float (ACK with commands), Base↔Plug (command/status)

## Build & Flash

- **Arduino IDE 2.x** required (no CLI build system)
- Base: ESP32 board, Float: ESP32 board, Plug: ESP8266 board
- Libraries: TFT_eSPI, XPT2046_Touchscreen, EmotiBit BMI160, OneWire, DallasTemperature
