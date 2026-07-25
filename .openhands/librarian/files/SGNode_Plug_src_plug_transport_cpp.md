# SGNode_Plug/src/plug_transport.cpp

**Type:** C++ Source
**Hash:** `f372885ae28e8186877f350cb3e12cf074fec23d06c26551872940d5ceaec1ff`

## Overview / Role

ESP-NOW transport layer for Plug communication

## Verified API & Symbols

### Functions

- `void macMatches (const uint8_t* left, const uint8_t* right) const`

- `void validFloat (float value, float minimum, float maximum)`

- `void validSeconds (uint16_t value, uint16_t minimum, uint16_t maximum)`

- `void begin (void)`

- `void sendStatus (sg_plug_status_v2_t* status)`

- `void onReceive (uint8_t* mac, uint8_t* data, uint8_t len)`

## Key Dependencies

**Local:**
- `SGNode_Plug/src/plug_transport.h`

**System/External:**
- `<ESP8266WiFi.h>`
- `<espnow.h>`
- `<user_interface.h>`

## Side Effects & Hardware Access

- ESP-NOW packet transmission
- ESP-NOW packet reception
- ESP-NOW peer registration
- WiFi mode configuration

## Change Risks

- **HIGH**: ESP-NOW transport affects communication

---
*Last modified (hash): f372885ae28e8186877f350cb3e12cf074fec23d06c26551872940d5ceaec1ff*