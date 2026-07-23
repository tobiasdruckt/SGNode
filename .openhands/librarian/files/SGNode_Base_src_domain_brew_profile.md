# SGNode_Base/src/domain/brew_profile.h

**Type:** C++ Header
**Hash:** `2bf1b492c7a21453322534c9263c1e0c7dc81097476c2b381d323894f42ee210`

## Role

Header file with declarations.

## Verified API / Symbols

- Structs: BrewProfile, YeastPerformanceSummary
- Classes: BrewProfileStore

- Functions: setDefaults, buildBatchId, profilePath, targetPath, logPath, eventsPath, activeBatchPath, ensureBatchDirectory, save, load

## Key Dependencies

- Arduino.h, FS.h

## Side Effects

- Temperature target calculation

## Tests

- No dedicated automated test identified

## Change Risks

- **MEDIUM**: Domain logic affects business rules

---
*Last modified (hash): 2bf1b492c7a21453322534c9263c1e0c7dc81097476c2b381d323894f42ee210*