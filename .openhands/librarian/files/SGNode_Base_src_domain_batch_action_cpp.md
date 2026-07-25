# SGNode_Base/src/domain/batch_action.cpp

**Type:** C++ Source
**Hash:** `5bb6a02fbbacd0b61f6fcf70c4fe90bb3cea46d1087506f3b95b181fed34c22d`

## Overview / Role

Batch profile management and persistence

## Verified API & Symbols

### Functions

- `void coldCrashDurationSeconds (const BrewProfile& profile) const`

- `void styleIsHoppy (const char* style) const`

- `void yeastIsLagerLike (const BrewProfile& profile) const`

- `void makeAction (BatchActionType type, int code, const char* title, const char* message) const`

- `void makeFutureAction (BatchActionType type, int code, const char* title, const char* message, unsigned long secondsUntilDue) const`

- `void formatDaysHours (unsigned long seconds, char* buffer, size_t bufferSize)`

- `void postPackageTemperatureAction (const BrewProfile& profile, unsigned long nowEpoch) const`

- `void residualCO2Volumes (float temperatureC)`

- `void formatPrimingSugarMessage (const BrewProfile& profile, char* message, size_t messageSize) const`

- `void applyStyleDefaults (BrewProfile* profile)`

- `void evaluate (const BrewProfile& profile, FermentationPhase phase, float attenuation, float currentSG, float gravityDeltaPerHour, unsigned long nowEpoch) const`

- `void applyDone (BrewProfile* profile, BatchActionType type, unsigned long nowEpoch)`

- `void applySkip (BrewProfile* profile, BatchActionType type, unsigned long nowEpoch)`

- `void eventName (BatchActionType type, bool done)`

## Key Dependencies

**Local:**
- `SGNode_Base/src/domain/batch_action.h`

**System/External:**
- `<math.h>`
- `<stdio.h>`
- `<string.h>`

## Side Effects & Hardware Access

- Temperature target calculation

## Change Risks

- **MEDIUM**: Domain logic affects business rules

---
*Last modified (hash): 5bb6a02fbbacd0b61f6fcf70c4fe90bb3cea46d1087506f3b95b181fed34c22d*