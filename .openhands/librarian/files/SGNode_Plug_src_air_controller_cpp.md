# SGNode_Plug/src/air_controller.cpp

**Type:** C++ Source
**Hash:** `b248bae18930b3aa64b92f2377d433a76ca59011eff188d4ce9633acebb0ee92`

## Overview / Role

Control logic for temperature or other systems

## Verified API & Symbols

### Functions

- `void config_ (config)`

- `void setConfig (const AirControllerConfig& config) const`

- `void update (float airC, bool airValid, float targetC, bool enabled, uint32_t nowMs)`

## Key Dependencies

**Local:**
- `SGNode_Plug/src/air_controller.h`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): b248bae18930b3aa64b92f2377d433a76ca59011eff188d4ce9633acebb0ee92*