# SGNode_Base/SGNode_Base.ino

**Type:** Arduino Sketch
**Hash:** `021bfb086bdfe16ee3be9bbf6a5c70ac53af4d45f4d652bd59bd9ad7b334ab67`

## Role

Base station entry point with ESP-NOW receive, UI management, and SD logging.

## Verified API / Symbols

- Structs: Theme
- Enums: DebugLevel

## Key Dependencies

- WiFi.h, esp_now.h, TFT_eSPI.h, FS.h, SD.h

## Side Effects

- ESP-NOW packet reception
- Touchscreen UI updates
- I2C/SPI bus communication
- Temperature target calculation
- Calibration coefficient computation

## Tests

- No dedicated automated test identified

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): 021bfb086bdfe16ee3be9bbf6a5c70ac53af4d45f4d652bd59bd9ad7b334ab67*