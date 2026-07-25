# SGNode_Base/src/domain/fermentation_state_machine.cpp

**Type:** C++ Source
**Hash:** `85c9c0fe3334b569d3dfbc181e86587ae184140bab53634ce565394a4c4d1c88`

## Overview / Role

State machine for fermentation tracking

## Verified API & Symbols

### Functions

- `void FermentationStateMachine (void)`

- `void reset (void)`

- `void update (float currentSG, float expectedFG, float attenuation, float gravityDeltaPerHour, float temperatureC, bool diacetylRestEnabled, unsigned long nowEpoch)`

- `void update (const BrewProfile& profile, float currentSG, float expectedFG, float attenuation, float gravityDeltaPerHour, float temperatureC, unsigned long nowEpoch) const`

- `void phaseName (FermentationPhase phase)`

## Key Dependencies

**Local:**
- `SGNode_Base/src/domain/fermentation_state_machine.h`

**System/External:**
- `<string.h>`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **HIGH**: State machine affects batch tracking

---
*Last modified (hash): 85c9c0fe3334b569d3dfbc181e86587ae184140bab53634ce565394a4c4d1c88*