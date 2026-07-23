# SGNode_Base/src/calculations/polynomial_calibration.h

**Type:** C++ Header
**Hash:** `ce07e40c0542bf44ae815f17addd6c4dff6402b6b49043b11548ab6c0a408a36`

## Role

Gravity calibration system (EEPROM-stored polynomial coefficients).

## Verified API / Symbols

- Structs: CalibrationCoefficients, CalibrationPoint

- Functions: initCalibration

## Key Dependencies

- Arduino.h
- EEPROM.h (for storing coefficients)


## Side Effects

- EEPROM read
- Calibration coefficient computation

## Tests

- No dedicated automated test identified

## Change Risks

- **HIGH**: Calibration affects all SG readings

---
*Last modified (hash): ce07e40c0542bf44ae815f17addd6c4dff6402b6b49043b11548ab6c0a408a36*