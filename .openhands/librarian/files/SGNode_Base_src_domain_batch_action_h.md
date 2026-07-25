# SGNode_Base/src/domain/batch_action.h

**Type:** C++ Header
**Hash:** `69b5bb81298f175b256d3641e80867a21e7a85e1f5823774d41d02a8a19d0d1f`

## Overview / Role

Batch profile management and persistence

## Verified API & Symbols

### Structs & Classes

- `struct BatchAction`
  - `BatchActionType type`
  - `int code`
  - `bool requiresChoice`
  - `unsigned long secondsUntilDue`
  - `char title[32]`
  - `char message[80]`

- `class BatchActionEngine`
  - void applyStyleDefaults (BrewProfile* profile) static
  - bool applyDone (BrewProfile* profile, BatchActionType type, unsigned long nowEpoch) static
  - bool applySkip (BrewProfile* profile, BatchActionType type, unsigned long nowEpoch) static
  - const char * eventName (BatchActionType type, bool done) const static

### Enums & Constants

- enum BatchActionType
  - `ACTION_NONE`
  - `ACTION_VERIFY_OG`
  - `ACTION_D_REST`
  - `ACTION_DRY_HOP`
  - `ACTION_REMOVE_DRY_HOP`
  - `ACTION_COLD_CRASH`
  - `ACTION_PACKAGE_START`
  - `ACTION_PACKAGE_COMPLETE`
  - `ACTION_POST_PACKAGE_TEMP`
  - `ACTION_FLOAT_ZERO_CAL`
  - `ACTION_FLOAT_IN_BREW`

### Macros

- `#define BATCH_ACTION_H #include "brew_profile.h"`

## Key Dependencies

**Local:**
- `SGNode_Base/src/domain/brew_profile.h`
- `SGNode_Base/src/domain/fermentation_state_machine.h`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Domain logic affects business rules

---
*Last modified (hash): 69b5bb81298f175b256d3641e80867a21e7a85e1f5823774d41d02a8a19d0d1f*