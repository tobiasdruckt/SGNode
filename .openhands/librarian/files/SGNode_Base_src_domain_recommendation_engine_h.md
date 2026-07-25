# SGNode_Base/src/domain/recommendation_engine.h

**Type:** C++ Header
**Hash:** `fa4882eb5244ddfa582fef8a0998f3cfdc4fa52632f430e79083fce5230b9a16`

## Overview / Role

Core functionality module

## Verified API & Symbols

### Structs & Classes

- `struct Recommendation`
  - `int code`
  - `char message[80]`

- `class RecommendationEngine`
  - Recommendation sensorIssue (const char* detail) static

### Macros

- `#define RECOMMENDATION_ENGINE_H #include "fermentation_state_machine.h"`

## Key Dependencies

**Local:**
- `SGNode_Base/src/domain/fermentation_state_machine.h`
- `SGNode_Base/src/domain/brew_profile.h`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Domain logic affects business rules

---
*Last modified (hash): fa4882eb5244ddfa582fef8a0998f3cfdc4fa52632f430e79083fce5230b9a16*