# SGNode_Base/src/domain/yeast_preset_repository.cpp

**Type:** C++ Source
**Hash:** `a295cf48c5417c1f6a9f884b584d4dad993fae32cf4b2e48aace3d0ee182153f`

## Overview / Role

Yeast preset management and persistence

## Verified API & Symbols

### Structs & Classes

- `struct StoredYeastPreset`
  - `char id[24]`
  - `char displayName[32]`
  - `char category[32]`
  - `int typicalAttenuationMin`
  - `int typicalAttenuationMax`
  - `int defaultAttenuation`
  - `char fermentationSpeed[16]`
  - `float lagPhaseHours`
  - `float typicalDurationHours`
  - `float recommendedTempMinC`
  - `float recommendedTempMaxC`
  - `char flocculation[16]`
  - `char curveTemplate[32]`
  - `bool diacetylRestRecommended`
  - `char notes[64]`

### Functions

- `void jsonEscapePrint (File& file, const char* text) const`

- `void extractString (const char* json, const char* key, char* out, size_t outSize) const`

- `void extractFloat (const char* json, const char* key, float fallback) const`

- `void extractInt (const char* json, const char* key, int fallback) const`

- `void extractBool (const char* json, const char* key, bool fallback) const`

- `void sanitizeId (const char* name, char* id, size_t idSize) const`

- `void bindCustomView (int index)`

- `void parseCustomPresetFile (const char* path, StoredYeastPreset* preset) const`

- `void invalidateCache (void)`

- `void reloadCustom (void)`

- `void count (void)`

- `void at (int index)`

- `void findById (const char* id) const`

- `void defaultPreset (void)`

- `void applyToProfile (const YeastPreset& preset, BrewProfile* profile) const`

- `void isCustom (const char* id) const`

- `void saveCustom (const YeastPreset& preset) const`

- `void deleteCustom (const char* id) const`

### Macros

- `#define MAX_CUSTOM_YEASTS 12`

## Key Dependencies

**Local:**
- `SGNode_Base/src/domain/yeast_preset_repository.h`
- `SGNode_Base/src/domain/../calculations/derived_calculations.h`

**System/External:**
- `<SD.h>`
- `<string.h>`
- `<stdlib.h>`

## Side Effects & Hardware Access

- SD card write (CSV/JSON)
- SD card read
- SD card file operations

## Change Risks

- **MEDIUM**: Domain logic affects business rules

---
*Last modified (hash): a295cf48c5417c1f6a9f884b584d4dad993fae32cf4b2e48aace3d0ee182153f*