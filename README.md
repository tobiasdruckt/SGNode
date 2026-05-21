# SGNode Intelligent Fermentation Assistant

**Release:** alpha 0.4.0  
**Platform:** ESP32 float + standalone ESP32 base station with 4-inch touchscreen

SGNode is a self-contained fermentation monitor and assistant. A battery-powered float measures tilt, temperature, and battery voltage. The base station receives data over ESP-NOW, stores batches on SD card, and provides all setup, charts, recommendations, and management directly on the built-in touchscreen.

No web UI, phone app, REST frontend, or external dashboard is used.

## Current Capabilities

### Base Station
- Native touchscreen UI with Live, Graph, Dashboard, Details, Battery, and More views
- Brew Wizard for batch name, style, volume, Brix, yeast, attenuation, and diacetyl rest
- Auto Mode with yeast behavior presets and custom yeast templates
- Brix to SG conversion, expected FG, attenuation, ABV, gravity slope, and ETA
- OG verification using first stable measured SG, with recipe/measured choice
- Target gravity, temperature, angle, and ABV charts
- Fermentation state machine and on-device recommendations
- Batch management: continue, copy, and delete stored batches
- Yeast management: create, edit, delete, and select custom yeast presets
- Local persistence on SD card under `/data/batches/<batch_id>/`
- Debug levels for quieter serial output
- Light/dark theme with persistent setting

### Float Unit
- BMI160 tilt measurement and BMP180 temperature measurement
- Polynomial gravity calibration stored in EEPROM
- ESP-NOW packet transmission with protocol version and CRC
- Battery voltage reporting
- Deep sleep between measurement cycles
- Calibration mode via hardware switch and base station workflow

## Hardware

### Float
- WeMos D32 ESP32 board with 18650 holder
- BMI160 IMU on I2C
- BMP180 temperature sensor on I2C
- 18650 Li-ion cell
- Calibration switch on GPIO12
- Waterproof enclosure

### Base
- 4.0-inch ESP32-32E touchscreen display module
- MicroSD card, FAT32
- 5V USB power supply
- Optional DS323x-compatible RTC on SDA GPIO32 and SCL GPIO25

See [hardware.md](hardware.md) and [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md) for pin details.

## Firmware Layout

```text
SGNode_Base/
  SGNode_Base.ino                 Base station firmware and UI orchestration
  brew_profile.*                  Batch model and SD persistence
  brew_wizard_controller.*         Native Brew Wizard flow
  yeast_preset*                   Built-in and custom yeast presets
  target_curve.*                  Expected fermentation curves
  fermentation_state_machine.*     Phase detection
  recommendation_engine.*         On-device recommendations
  eta_predictor.*                 Packaging ETA
  derived_calculations.*          FG, attenuation, ABV, rates
  og_verifier.*                   Recipe vs measured OG logic
  ui_*                            Shared touchscreen UI helpers

SGNode_Float/
  SGNode_Float.ino                Float firmware
  polynomial_calibration.h         Float calibration logic

SGNode_Shared/
  sg_protocol.h                   ESP-NOW payload format
```

## SD Card Structure

```text
/data/
  batches/
    batch_001/
      profile.json
      target.json
      log.csv
  yeast/
    <custom_yeast>.json
  yeasts.txt
  yeast_history.json
```

`log.csv` contains timestamp, epoch, uptime, angle, density, temperature, battery, fermentation state, attenuation, ABV, and recommendation code. Historical data is loaded after reboot and should be visible immediately.

## Setup

1. Install Arduino IDE 2.x and ESP32 board support.
2. Install libraries:
   - TFT_eSPI
   - XPT2046_Touchscreen
   - EmotiBit BMI160
   - Adafruit BMP085
   - SD, FS, SPI, Wire, EEPROM
3. Configure TFT_eSPI for the 4.0-inch ESP32-32E display.
4. Flash `SGNode_Base/SGNode_Base.ino`.
5. Temporarily set `SGNODE_DEBUG_LEVEL` to `DEBUG_INFO` and read the base station MAC address from Serial Monitor.
6. Set that MAC in `SGNode_Float/SGNode_Float.ino`.
7. Flash the float.
8. Insert a FAT32 MicroSD card into the base station.
9. Use the touchscreen Brew Wizard to create the first batch.

## Normal Operation

1. Create or continue a batch on the base station.
2. Place the calibrated float in the fermenter.
3. The float wakes, measures, transmits, and returns to sleep.
4. The base station updates the Live screen, charts, dashboard, recommendations, ETA, and CSV log.
5. Use More > Manage Brew to continue, copy, or delete batches.
6. Use More > Manage Yeast to maintain custom yeast presets.

## Calibration

Calibration is started from the base station and executed by the float.

1. Turn on the float calibration switch.
2. Open More > Calibration on the base station.
3. Record the offset on a level surface.
4. Record SG points in known solutions.
5. Apply calibration to save polynomial coefficients to float EEPROM.
6. Turn off the calibration switch to return to deep sleep operation.

Current calibration uses the existing float firmware. Adding new optional calibration points normally requires float firmware changes because the float owns the polynomial calculation and EEPROM format.

## Debug Output

Base station serial logging is controlled in `SGNode_Base.ino`:

```cpp
#define SGNODE_DEBUG_LEVEL DEBUG_ERROR
```

Levels:
- `DEBUG_QUIET`
- `DEBUG_ERROR`
- `DEBUG_INFO`
- `DEBUG_VERBOSE`

Use `DEBUG_INFO` temporarily for setup details such as the base station MAC address. Use `DEBUG_VERBOSE` only when diagnosing packet, SD, graph, or calibration behavior.

## Troubleshooting

### Historical graphs are empty after reboot
- Check Details > System:
  - `SD OK` means the active log path is usable.
  - `CSV L/T` means loaded rows / total rows.
  - `Buf` means unsaved RAM buffer entries.
- If `T` is correct but `L` is low, rows may be outside the selected time range or older than the active batch start timestamp.
- If `Buf` is high before reboot, those rows have not been flushed to SD yet.

### Old data appears in a new batch
- Confirm the active batch in Manage Brew.
- New alpha 0.4.0 builds avoid reusing existing batch directories and filter rows older than the active batch creation time.

### No float data received
- Verify the base station MAC in the float sketch.
- Confirm both devices are powered.
- Check ESP-NOW channel and antenna placement.
- Use `DEBUG_INFO` or `DEBUG_VERBOSE` temporarily.

### SD card issues
- Use FAT32.
- Try a different MicroSD card.
- Insert the card before boot.
- Check that `/data/batches/<batch_id>/log.csv` exists.

### Battery drains too fast
- Verify the float is entering deep sleep.
- Keep calibration mode off during normal use.
- Confirm measurement delay/sleep interval settings.
- Check the temperature sensor and IMU power states if hardware current remains high.

## Release Notes

See [RELEASE_NOTES_v0.4.0-alpha.md](RELEASE_NOTES_v0.4.0-alpha.md).

## Status

alpha 0.4.0 is ready for hands-on testing. The system is feature-rich but still needs longer fermentation runs to validate battery life, SD flush behavior, fermentation phase thresholds, and UI edge cases.
