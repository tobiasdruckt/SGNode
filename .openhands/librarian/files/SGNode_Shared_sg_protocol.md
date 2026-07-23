# SGNode_Shared/sg_protocol.h

**Type:** C++ Header
**Hash:** `1a1c39d03d752bb6043717f632a80a7701d9321eb9f4254b5f8468d61444f30f`

## Role

Protocol definitions for ESP-NOW communication (Base↔Float/Plug).

## Verified API / Symbols

- `payload_t` - Float data packet (26 bytes)
- `sg_plug_command_t` - Plug command (107 bytes)
- `sg_plug_status_t` - Plug status report (61 bytes)
- `ack_packet_t` - ACK packet with commands (7 bytes)
- `calib_command_t`, `calib_response_t`, `calib_coeffs_t`
- `sg_crc16()` - CRC16-CCITT checksum function

Constants:
- `SG_PROTOCOL_VERSION = 2`
- `SG_ACK_COMMAND_ZERO_CALIBRATE = 1`

- Functions: sg_crc16

## Key Dependencies

- Arduino.h (cstdint, cstddef)


## Side Effects

- Sensor reading
- Temperature target calculation
- Calibration coefficient computation

## Tests

- No dedicated automated test identified

## Change Risks

- **HIGH**: Protocol changes break compatibility

---
*Last modified (hash): 1a1c39d03d752bb6043717f632a80a7701d9321eb9f4254b5f8468d61444f30f*