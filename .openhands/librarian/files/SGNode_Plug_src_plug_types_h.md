# SGNode_Plug/src/plug_types.h

**Type:** C++ Header
**Hash:** `ce8625bc8ce176ee42a78beea442ccf4ed55586093ea766cdf19b6ec3cb79450`

## Overview / Role

Core functionality module

## Verified API & Symbols

### Structs & Classes

- `struct SensorSnapshot`
  - `float airC = NAN`
  - `float beerC = NAN`
  - `bool airValid = false`
  - `bool beerValid = false`
  - `uint32_t sampledAtMs = 0`

- `class PlugControlMode`

## Key Dependencies

**System/External:**
- `<Arduino.h>`
- `<sg_protocol.h>`

## Side Effects & Hardware Access

- Sensor reading

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): ce8625bc8ce176ee42a78beea442ccf4ed55586093ea766cdf19b6ec3cb79450*