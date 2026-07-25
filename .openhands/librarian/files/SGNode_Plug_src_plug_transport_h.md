# SGNode_Plug/src/plug_transport.h

**Type:** C++ Header
**Hash:** `071491b33f29388db606c98974c5e80d810b3e8e6d416629234cba9f1407e810`

## Overview / Role

ESP-NOW transport layer for Plug communication

## Verified API & Symbols

### Structs & Classes

- `class PlugTransport`
  - bool begin (void)
  - bool hasCommand (void)
  - sg_plug_command_t command (void)
  - uint32_t lastCommandMs (void)
  - bool sendStatus (sg_plug_status_v2_t* status)
  - [private]
  - void onReceive (uint8_t* mac, uint8_t* data, uint8_t len) static

## Key Dependencies

**System/External:**
- `<Arduino.h>`
- `<sg_protocol.h>`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **HIGH**: ESP-NOW transport affects communication

---
*Last modified (hash): 071491b33f29388db606c98974c5e80d810b3e8e6d416629234cba9f1407e810*