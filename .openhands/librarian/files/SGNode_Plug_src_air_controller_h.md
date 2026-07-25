# SGNode_Plug/src/air_controller.h

**Type:** C++ Header
**Hash:** `8307e315ba311d8ae50e1e14f3aa872aa893262df997e26e768ca7ebc397dfbf`

## Overview / Role

Control logic for temperature or other systems

## Verified API & Symbols

### Structs & Classes

- `struct AirControllerConfig`
  - `float turnOffAboveTargetC = 0.5f`
  - `float turnOnAboveTargetC = 1.1f`
  - `uint32_t minimumOnMs = 120000`
  - `uint32_t minimumOffMs = 300000`

- `class AirController`
  - AirControlle r (const AirControllerConfig& config = AirControllerConfig()
  - bool update (float airC, bool airValid, float targetC, bool enabled, uint32_t nowMs)
  - void setConfig (const AirControllerConfig& config)
  - bool relayOn (void)
  - uint32_t lastChangeMs (void)

## Key Dependencies

**System/External:**
- `<Arduino.h>`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): 8307e315ba311d8ae50e1e14f3aa872aa893262df997e26e768ca7ebc397dfbf*