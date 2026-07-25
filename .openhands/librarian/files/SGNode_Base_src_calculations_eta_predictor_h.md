# SGNode_Base/src/calculations/eta_predictor.h

**Type:** C++ Header
**Hash:** `9bb70669aa0630c0d5a6b694f5b093f1069b2dcb977ac4a4fb863eb5072ae4ec`

## Overview / Role

Mathematical calculations for brewing metrics

## Verified API & Symbols

### Structs & Classes

- `struct ETAResult`
  - `unsigned long secondsToFG`
  - `unsigned long secondsToPackaging`
  - `bool valid`
  - `bool presetBased`
  - `uint8_t confidencePercent`

- `class ETAPredictor`
  - ETAResult predict (float currentSG, float expectedFG, float gravityDeltaPerHour) static

### Macros

- `#define ETA_PREDICTOR_H #include "../domain/brew_profile.h"`

## Key Dependencies

**Local:**
- `SGNode_Base/src/calculations/../domain/brew_profile.h`

**System/External:**
- `<stdint.h>`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Calculation changes affect data accuracy

---
*Last modified (hash): 9bb70669aa0630c0d5a6b694f5b093f1069b2dcb977ac4a4fb863eb5072ae4ec*