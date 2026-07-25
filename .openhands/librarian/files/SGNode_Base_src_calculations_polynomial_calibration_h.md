# SGNode_Base/src/calculations/polynomial_calibration.h

**Type:** C++ Header
**Hash:** `ce07e40c0542bf44ae815f17addd6c4dff6402b6b49043b11548ab6c0a408a36`

## Overview / Role

Mathematical calculations for brewing metrics

## Verified API & Symbols

### Structs & Classes

- `struct CalibrationCoefficients`
  - `float coeff3`
  - `float coeff2`
  - `float coeff1`
  - `float coeff0`
  - `bool isValid`

- `struct CalibrationPoint`
  - `float tilt`
  - `float gravity`
  - `float temperature`

### Functions

- `void initCalibration (void)`

- `void addCalibrationPoint (float tilt, float gravity, float temperature = 20.0)`

- `void calculatePolynomialCoefficients (void)`

- `void saveCalibrationCoefficients (void)`

- `void calculateGravity (float tilt)`

- `void resetCalibration (void)`

- `void testCalibrationAccuracy (void)`

- `void testPolynomialRange (void)`

- `void isCalibrationValid (void)`

- `void printCalibrationCoefficients (void)`

### Macros

- `#define POLYNOMIAL_CALIBRATION_H #define DEBUG_CALIBRATION 0`

- `#define DBG_PRINT(...) Serial.print(__VA_ARGS__)`

- `#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)`

- `#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)`

- `#define DBG_PRINT(...) #define DBG_PRINTLN(...)`

- `#define DBG_PRINTF(...) #endif`

- `#define EEPROM_CALIB_MAGIC 0`

- `#define EEPROM_COEFF3 4`

- `#define EEPROM_COEFF2 8`

- `#define EEPROM_COEFF1 12`

- `#define EEPROM_COEFF0 16`

- `#define EEPROM_CALIB_VERSION 20`

- `#define EEPROM_NORM_OFFSET 21`

- `#define EEPROM_NORM_SCALE 25`

- `#define CALIB_MAGIC 0x43414C49 // "CALI" in hex`

- `#define MAX_CALIB_POINTS 10`

## Key Dependencies

**System/External:**
- `<Arduino.h>`
- `<EEPROM.h>`

## Side Effects & Hardware Access

- EEPROM write (persistent storage)
- EEPROM read
- EEPROM initialization
- EEPROM commit (flush)
- Calibration coefficient computation
- Calibration data persistence
- Serial port initialization
- Serial output

## Change Risks

- **HIGH**: Calibration affects all SG readings

---
*Last modified (hash): ce07e40c0542bf44ae815f17addd6c4dff6402b6b49043b11548ab6c0a408a36*