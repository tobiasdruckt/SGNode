# SGNode_Base/src/domain/yeast_preset_repository.h

**Type:** C++ Header
**Hash:** `1a147f964cd8618a893252e45a2800361be83310e576e99de19c2c4fcf8ad808`

## Overview / Role

Yeast preset management and persistence

## Verified API & Symbols

### Structs & Classes

- `class YeastPresetRepository`
  - void invalidateCache (void) static
  - bool reloadCustom (void) static
  - int count (void) static
  - void applyToProfile (const YeastPreset& preset, BrewProfile* profile) static
  - bool isCustom (const char* id) static
  - bool saveCustom (const YeastPreset& preset) static
  - bool deleteCustom (const char* id) static

### Macros

- `#define YEAST_PRESET_REPOSITORY_H #include "yeast_preset.h"`

## Key Dependencies

**Local:**
- `SGNode_Base/src/domain/yeast_preset.h`
- `SGNode_Base/src/domain/brew_profile.h`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Domain logic affects business rules

---
*Last modified (hash): 1a147f964cd8618a893252e45a2800361be83310e576e99de19c2c4fcf8ad808*