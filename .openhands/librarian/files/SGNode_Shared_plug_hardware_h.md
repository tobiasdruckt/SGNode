# SGNode_Shared/plug_hardware.h

**Type:** C++ Header
**Hash:** `9040c98aef18a2a87f8e2de6f7bfb239a11960e1cd32cc37a2c1f87b4a31860b`

## Overview / Role

Hardware pin definitions and inline functions

## Verified API & Symbols

### Functions

- `void beginRelayOff (void)`

- `void setRelay (bool on)`

## Key Dependencies

**System/External:**
- `<Arduino.h>`

## Side Effects & Hardware Access

- Relay control (fridge compressor)
- I2C bus communication
- OneWire bus communication
- Serial port initialization

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): 9040c98aef18a2a87f8e2de6f7bfb239a11960e1cd32cc37a2c1f87b4a31860b*