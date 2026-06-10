# SGNode Intelligent Fermentation Assistant

**Release:** alpha 0.5.0  
**Platform:** ESP32 float + standalone ESP32 base station with 4-inch touchscreen

SGNode is a self-contained fermentation monitor and assistant. A battery-powered float measures tilt, temperature, and battery voltage. The base station receives data over ESP-NOW, stores batches on SD card, and provides all setup, charts, recommendations, and management directly on the built-in touchscreen.

No web UI, phone app, REST frontend, or external dashboard is used.

## Current Status

SGNode remains in **alpha** while SGNode Plug temperature control is still awaiting real water/fridge validation. Float/Base have completed real-batch use, but the Plug controller should pass water tests before the project moves to beta.

## Current Capabilities

### Base Station
- Native touchscreen UI with Live, Graph, Dashboard, Details, Battery, and More views
- Brew Wizard for batch name, style, volume, Brix, yeast, attenuation, and diacetyl rest
- Auto Mode with yeast behavior presets and custom yeast templates
- Brix to SG conversion, expected FG, attenuation, ABV, gravity slope, and ETA
- OG verification using first stable measured SG, with recipe/measured choice
- Batch-start float zero calibration with explicit "put float in brew" confirmation before logging
- Target gravity, temperature, angle, and ABV charts
- Fermentation state machine and on-device recommendations
- Batch management: continue, copy, and delete stored batches
- Yeast management: create, edit, delete, and select custom yeast presets
- Local persistence on SD card under `/data/batches/<batch_id>/`
- Debug levels for quieter serial output
- Light/dark theme with persistent setting

### Float Unit
- BMI160 tilt measurement and BMI160 chip-temperature reporting
- Polynomial gravity calibration stored in EEPROM
- ESP-NOW packet transmission with protocol version and CRC
- Battery voltage reporting
- Deep sleep between measurement cycles
- Calibration mode via hardware switch and base station workflow

## Hardware

### Float
- WeMos D32 ESP32 board with 18650 holder
- BMI160 IMU on I2C
- BMP180/GY-68 removed in the current low-power build
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
  src/
    assets/                       Boot logo data
    calculations/                 Brix, gravity, ETA, OG, and target curve logic
    domain/                       Batch model, yeast presets, state, and recommendations
    test/                         Optional serial UI test harness
    ui/                           Brew Wizard and shared touchscreen UI helpers

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
   - Adafruit BMP085, still included by the current float sketch for compatibility even though the BMP180 sensor is no longer installed
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
2. Follow the Dashboard action `Level Float` if a batch-start zero calibration is required.
3. Place the float in the fermenter only after zero calibration is complete.
4. Confirm `Put Float In Brew` on the Dashboard.
5. The float wakes, measures, transmits, and returns to sleep.
6. The base station updates the Live screen, charts, dashboard, recommendations, ETA, and CSV log.
7. Use More > Manage Brew to continue, copy, or delete batches.
8. Use More > Manage Yeast to maintain custom yeast presets.

## Batch Start Workflow

SGNode intentionally separates preparation from the real batch start.

1. Brew Wizard creates the batch profile and target curve.
2. Dashboard shows `Level Float`.
3. Put the float on a level reference surface and press `READY`.
4. Base waits for the next normal float packet and sends a zero-calibration command in the ESP-NOW ACK.
5. Float performs a short accelerometer zero calibration, stores the offset, and reports success in a later packet.
6. Dashboard then shows `Put Float In Brew`.
7. Place the float in the wort/beer and press `DONE`.

Before `Put Float In Brew` is confirmed, incoming float packets are allowed to update the live display but are not written to the active batch CSV and are not used for OG detection or fermentation state changes. This prevents handling, air, and table measurements from contaminating the batch history.

When `Put Float In Brew` is confirmed:

- `createdAt` is reset to the confirmation time.
- the RAM write buffer is cleared.
- the active `log.csv` is recreated with only the header.
- the target curve is regenerated from the real batch start.
- OG stability detection and normal SD logging begin.

## Calibration

Calibration is started from the base station and executed by the float.

1. Turn on the float calibration switch.
2. Open More > Calibration on the base station.
3. Record the offset on a level surface.
4. Record SG points in known solutions.
5. Apply calibration to save polynomial coefficients to float EEPROM.
6. Turn off the calibration switch to return to deep sleep operation.

Current calibration uses the existing float firmware. Adding new optional calibration points normally requires float firmware changes because the float owns the polynomial calculation and EEPROM format.

### Batch-Start Zero Calibration

The batch-start zero calibration is separate from polynomial SG calibration. It only updates the accelerometer zero offset used for tilt.

- It runs in normal float operation, not in the long calibration mode.
- The float remains on a level reference surface.
- Base sends the zero-calibration command inside the normal ACK/downlink packet.
- Float uses a short sample window, saves the offset if the raw acceleration is plausible and stable, and then returns to sleep.
- If zero calibration fails, keep the float level and repeat the Dashboard action.

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
- Current builds avoid reusing existing batch directories and filter rows older than the active batch creation time.
- For a new batch, confirm `Put Float In Brew` before expecting CSV rows. Data received before that confirmation is intentionally not logged.

### New batch receives float packets but log stays empty
- This is expected until `Put Float In Brew` has been confirmed.
- The Live screen can show current float data before the batch is armed.
- Confirm the Dashboard action after the float is actually in the wort/beer.

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
- Current reference hardware reached about `0.95 mA` deep sleep after removing the BMP180/GY-68 and isolating the USB-UART chip.
- The ME6220 regulator swap did not provide enough improvement to justify the rework by itself.
- Check IMU suspend state and USB-UART leakage first if hardware current remains high.

## Release Notes

See [RELEASE_NOTES_v0.5.0-alpha.md](RELEASE_NOTES_v0.5.0-alpha.md).

## Status

alpha 0.5.0 is ready for hands-on testing. Float/Base have completed a first real batch, while SGNode Plug remains alpha until water/fridge validation is complete.
