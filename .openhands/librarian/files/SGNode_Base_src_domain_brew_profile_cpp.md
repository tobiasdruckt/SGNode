# SGNode_Base/src/domain/brew_profile.cpp

**Type:** C++ Source
**Hash:** `3493903bdd746e02acba991dd531f85dd2aff5fbe26d4116defa473a74c4fef7`

## Overview / Role

Brew profile data structure and persistence

## Verified API & Symbols

### Functions

- `void jsonEscapePrint (File& file, const char* text) const`

- `void csvEscapePrint (File& file, const char* text) const`

- `void extractString (const char* json, const char* key, char* out, size_t outSize) const`

- `void extractFloat (const char* json, const char* key, float fallback) const`

- `void extractInt (const char* json, const char* key, int fallback) const`

- `void extractULong (const char* json, const char* key, unsigned long fallback) const`

- `void extractBool (const char* json, const char* key, bool fallback) const`

- `void setDefaults (BrewProfile* profile)`

- `void buildBatchId (int number, char* buffer, size_t bufferSize)`

- `void profilePath (const char* batchId, char* buffer, size_t bufferSize) const`

- `void targetPath (const char* batchId, char* buffer, size_t bufferSize) const`

- `void logPath (const char* batchId, char* buffer, size_t bufferSize) const`

- `void eventsPath (const char* batchId, char* buffer, size_t bufferSize) const`

- `void activeBatchPath (char* buffer, size_t bufferSize)`

- `void ensureBatchDirectory (const char* batchId) const`

- `void saveActiveBatchId (const char* batchId) const`

- `void loadActiveBatchId (char* batchId, size_t bufferSize)`

- `void clearActiveBatchId (void)`

- `void save (const BrewProfile& profile) const`

- `void load (const char* batchId, BrewProfile* profile) const`

- `void loadFromPath (const char* path, BrewProfile* profile) const`

- `void saveYeastHistory (const char* yeastName) const`

- `void loadYeastHistory (char names[][32], int maxNames)`

- `void appendYeastPerformance (const YeastPerformanceSummary& summary) const`

- `void appendBatchEvent (const char* batchId, unsigned long epoch, const char* eventType, const char* message, float value) const`

## Key Dependencies

**Local:**
- `SGNode_Base/src/domain/brew_profile.h`

**System/External:**
- `<SD.h>`
- `<string.h>`
- `<stdlib.h>`

## Side Effects & Hardware Access

- SD card write (CSV/JSON)
- SD card read
- SD card file operations
- Temperature target calculation
- Calibration data persistence

## Change Risks

- **MEDIUM**: Domain logic affects business rules

---
*Last modified (hash): 3493903bdd746e02acba991dd531f85dd2aff5fbe26d4116defa473a74c4fef7*