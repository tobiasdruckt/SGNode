# SGNode_Base/src/calculations/target_curve.cpp

**Type:** C++ Source
**Hash:** `6b31f77f18af237629cf70b9c5afe39a22df04ad2aebdc1bb051082f4b19d600`

## Overview / Role

Mathematical calculations for brewing metrics

## Verified API & Symbols

### Functions

- `void clamp01 (float v)`

- `void fermentationStartOffsetHours (const BrewProfile& profile) const`

- `void modelHourForChartHour (const BrewProfile& profile, float chartHour) const`

- `void curveParams (const BrewProfile& profile, float* lagH, float* activeH, float* finishH, float* activeShare) const`

- `void expectedGravityAtHour (const BrewProfile& profile, float hour) const`

- `void expectedAttenuationAtHour (const BrewProfile& profile, float hour) const`

- `void generateAndSave (const BrewProfile& profile) const`

## Key Dependencies

**Local:**
- `SGNode_Base/src/calculations/target_curve.h`

**System/External:**
- `<SD.h>`
- `<string.h>`

## Side Effects & Hardware Access

- SD card write (CSV/JSON)
- SD card file operations

## Change Risks

- **MEDIUM**: Calculation changes affect data accuracy

---
*Last modified (hash): 6b31f77f18af237629cf70b9c5afe39a22df04ad2aebdc1bb051082f4b19d600*