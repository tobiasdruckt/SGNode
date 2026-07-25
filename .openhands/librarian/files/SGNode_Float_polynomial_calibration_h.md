# SGNode_Float/polynomial_calibration.h

**Type:** C++ Header
**Hash:** `c860828f3335f16a3f4f0f5696c0c8c764af20b02a3d7d745f6d3b03f63c7698`

## Overview / Role

3rd degree polynomial calibration for gravity measurement

## Verified API & Symbols

### Structs & Classes

- `struct CalibrationCoefficients`
  - `float coeff3`
  - `float coeff2`
  - `float coeff1`
  - `float coeff0`
  - `bool isValid`
  - `float norm_min`
  - `float norm_range`

- `struct SensorOffsets`
  - `float offsetX`
  - `float offsetY`
  - `float offsetZ`
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

- `void isCalibrationValid (void)`

- `void printCalibrationCoefficients (void)`

- `void initSensorOffsets (void)`

- `void calibrateSensorOffset (void)`

- `void loadSensorOffsets (void)`

- `void saveSensorOffsets (void)`

- `void applySensorOffsets (int &ax, int &ay, int &az)`

- `void isSensorOffsetValid (void)`

- `void printSensorOffsets (void)`

### Macros

- `#define POLYNOMIAL_CALIBRATION_H #include <Arduino.h>`

- `#define EEPROM_CALIB_MAGIC 0`

- `#define EEPROM_COEFF3 4`

- `#define EEPROM_COEFF2 8`

- `#define EEPROM_COEFF1 12`

- `#define EEPROM_COEFF0 16`

- `#define EEPROM_CALIB_VERSION 20`

- `#define EEPROM_POLY_DEGREE 21`

- `#define EEPROM_NORM_MIN 22`

- `#define EEPROM_NORM_RANGE 26`

- `#define EEPROM_OFFSET_X 30`

- `#define EEPROM_OFFSET_Y 34`

- `#define EEPROM_OFFSET_Z 38`

- `#define EEPROM_OFFSET_VALID 42`

- `#define EEPROM_OFFSET_MAGIC 46`

- `#define CALIB_MAGIC 0x43414C49 // "CALI" in hex`

- `#define OFFSET_MAGIC 0x4F464654 // "OFFT" in hex`

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
- Sensor reading
- Calibration coefficient computation
- Calibration data persistence
- Execution delay
- Serial port initialization
- Serial output

## Change Risks

- **HIGH**: Calibration affects all SG readings

---
*Last modified (hash): c860828f3335f16a3f4f0f5696c0c8c764af20b02a3d7d745f6d3b03f63c7698*