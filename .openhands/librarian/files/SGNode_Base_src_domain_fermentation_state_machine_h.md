# SGNode_Base/src/domain/fermentation_state_machine.h

**Type:** C++ Header
**Hash:** `c94df5f87726f569a1de4751b6daa8bea19875daa21fbb661194700243f60946`

## Overview / Role

State machine for fermentation tracking

## Verified API & Symbols

### Structs & Classes

- `class FermentationStateMachine`
  - FermentationStateMachin e (void)
  - void reset (void)
  - FermentationPhase phase (void)
  - const char * phaseName (void) const
  - const char * phaseName (FermentationPhase phase) const static

### Enums & Constants

- enum FermentationPhase
  - `FERMENTATION_IDLE`
  - `FERMENTATION_PITCHED`
  - `FERMENTATION_LAG_PHASE`
  - `FERMENTATION_ACTIVE`
  - `FERMENTATION_DIACETYL_REST_READY`
  - `FERMENTATION_FINAL_GRAVITY_STABLE`
  - `FERMENTATION_READY_TO_PACKAGE`
  - `FERMENTATION_COMPLETED`

### Macros

- `#define FERMENTATION_STATE_MACHINE_H #include "brew_profile.h"`

## Key Dependencies

**Local:**
- `SGNode_Base/src/domain/brew_profile.h`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **HIGH**: State machine affects batch tracking

---
*Last modified (hash): c94df5f87726f569a1de4751b6daa8bea19875daa21fbb661194700243f60946*