# SGNode_Float/SGNode_Float.ino

**Type:** Arduino Sketch
**Hash:** `3f178f02a19c8e4524398c8d33c996b82d669cfd9eecfabeeb3e3d4d054ec34d`

## Role

Float unit entry point with IMU measurement, ESP-NOW transmit, and deep sleep.

## Verified API / Symbols

- Structs: StoredPayload, PendingStableCalibrationPoint, PendingStableOffsetCalibration
- Enums: State, CalibState

## Key Dependencies

- WiFi.h, esp_now.h, Wire.h, BMI160Gen.h, polynomial_calibration.h

## Side Effects

- ESP-NOW packet transmission
- ESP-NOW packet reception
- EEPROM write (persistent storage)
- EEPROM read
- SD card write (CSV/JSON)
- SD card read
- Sensor reading
- I2C/SPI bus communication
- Temperature target calculation
- Calibration coefficient computation

## Tests

- No dedicated automated test identified

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): 3f178f02a19c8e4524398c8d33c996b82d669cfd9eecfabeeb3e3d4d054ec34d*