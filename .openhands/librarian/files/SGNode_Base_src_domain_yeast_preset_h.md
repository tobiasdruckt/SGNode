# SGNode_Base/src/domain/yeast_preset.h

**Type:** C++ Header
**Hash:** `b73fa077c95b8817bcb092ea003f989cab3516cdc6004e01fa5ec9d90076fd9a`

## Overview / Role

Yeast preset management and persistence

## Verified API & Symbols

### Structs & Classes

- `struct YeastPreset`
  - `const char* id`
  - `const char* displayName`
  - `const char* category`
  - `int typicalAttenuationMin`
  - `int typicalAttenuationMax`
  - `int defaultAttenuation`
  - `const char* fermentationSpeed`
  - `float lagPhaseHours`
  - `float typicalDurationHours`
  - `float recommendedTempMinC`
  - `float recommendedTempMaxC`
  - `const char* flocculation`
  - `const char* curveTemplate`
  - `bool diacetylRestRecommended`
  - `const char* notes`

### Macros

- `#define YEAST_PRESET_H struct YeastPreset {`

## Key Dependencies

- No external dependencies

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Domain logic affects business rules

---
*Last modified (hash): b73fa077c95b8817bcb092ea003f989cab3516cdc6004e01fa5ec9d90076fd9a*