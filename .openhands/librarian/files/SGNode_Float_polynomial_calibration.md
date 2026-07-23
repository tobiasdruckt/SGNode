# SGNode_Float/polynomial_calibration.h

**Type:** C++ Header
**Hash:** `c860828f3335f16a3f4f0f5696c0c8c764af20b02a3d7d745f6d3b03f63c7698`

## Role

Gravity calibration system (EEPROM-stored polynomial coefficients).

## Verified API / Symbols

- Structs: CalibrationCoefficients, SensorOffsets, CalibrationPoint

- Functions: initCalibration, addCalibrationPoint

## Key Dependencies

- Arduino.h
- EEPROM.h (for storing coefficients)


## Side Effects

- EEPROM read
- Sensor reading
- Calibration coefficient computation

## Tests

- No dedicated automated test identified

## Change Risks

- **HIGH**: Calibration affects all SG readings

---
*Last modified (hash): c860828f3335f16a3f4f0f5696c0c8c764af20b02a3d7d745f6d3b03f63c7698*