# SGNode_Base/src/calculations/derived_calculations.h

**Type:** C++ Header
**Hash:** `37064a7fb28dfe34bbdf2ebc0d15bf093049260e5271c588c499718ba0770b59`

## Overview / Role

Mathematical calculations for brewing metrics

## Verified API & Symbols

### Structs & Classes

- `struct DerivedMetrics`
  - `float expectedFinalGravity`
  - `float currentAttenuation`
  - `float estimatedABV`
  - `float gravityDeltaPerHour`
  - `float fermentationRate`

- `class DerivedCalculations`
  - float expectedFG (float og, int apparentAttenuationPercent) static
  - float apparentAttenuation (float effectiveOG, float currentSG) static
  - float abv (float effectiveOG, float currentSG) static
  - float gravityDeltaPerHour (float previousSG, float currentSG, unsigned long deltaSeconds) static

### Macros

- `#define DERIVED_CALCULATIONS_H struct DerivedMetrics {`

## Key Dependencies

- No external dependencies

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Calculation changes affect data accuracy

---
*Last modified (hash): 37064a7fb28dfe34bbdf2ebc0d15bf093049260e5271c588c499718ba0770b59*