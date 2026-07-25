# SGNode_Base/src/calculations/target_curve.h

**Type:** C++ Header
**Hash:** `1c5d0c6030944191cfbcedbfc1d03d2b40349b1e07c24ff98de612adee10b815`

## Overview / Role

Mathematical calculations for brewing metrics

## Verified API & Symbols

### Structs & Classes

- `class TargetCurveGenerator`
  - bool generateAndSave (const BrewProfile& profile) static
  - float expectedGravityAtHour (const BrewProfile& profile, float hour) static
  - float expectedAttenuationAtHour (const BrewProfile& profile, float hour) static

### Macros

- `#define TARGET_CURVE_H #include "../domain/brew_profile.h"`

## Key Dependencies

**Local:**
- `SGNode_Base/src/calculations/../domain/brew_profile.h`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Calculation changes affect data accuracy

---
*Last modified (hash): 1c5d0c6030944191cfbcedbfc1d03d2b40349b1e07c24ff98de612adee10b815*