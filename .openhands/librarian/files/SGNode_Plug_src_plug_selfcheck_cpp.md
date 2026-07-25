# SGNode_Plug/src/plug_selfcheck.cpp

**Type:** C++ Source
**Hash:** `f7e16b5fd0735f0429bd1fba9a79b6136e53fec66378537161f92474696c5c88`

## Overview / Role

Core functionality module

## Verified API & Symbols

### Functions

- `void check (Print& output, bool condition, const __FlashStringHelper* name) const`

- `void runPlugSelfChecks (Print& output)`

## Key Dependencies

**Local:**
- `SGNode_Plug/src/plug_selfcheck.h`
- `SGNode_Plug/src/air_controller.h`
- `SGNode_Plug/src/beer_pi_controller.h`
- `SGNode_Plug/src/relay_history.h`

## Side Effects & Hardware Access

- Sensor reading
- Temperature target calculation

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): f7e16b5fd0735f0429bd1fba9a79b6136e53fec66378537161f92474696c5c88*