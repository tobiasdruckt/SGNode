# SGNode_Shared/sg_protocol.h

**Type:** C++ Header
**Hash:** `4a349e41c279633b6493caa3db926265a8265f92a8558487be1d6c10f8ca56f1`

## Overview / Role

Protocol definitions for ESP-NOW communication (Base↔Float/Plug)

## Verified API & Symbols

### Structs & Classes

- `struct payload_t`
  - (packed)
  - `uint16_t sequence_id`
  - `uint32_t uptime_s`
  - `float angle`
  - `float density`
  - `float temperature`
  - `float battery_voltage`
  - `uint8_t flags`
  - `uint16_t crc`

- `struct calib_command_t`
  - (packed)
  - `uint8_t command`
  - `float target_sg`
  - `uint8_t request_id`

- `struct ack_packet_t`
  - (packed)
  - `uint8_t packet_type`
  - `uint16_t sequence_id`
  - `uint16_t highest_seen`
  - `uint8_t command`
  - `uint8_t command_id`

- `struct calib_response_t`
  - (packed)
  - `uint8_t response_type`
  - `float angle`
  - `float sg`
  - `uint8_t request_id`
  - `char message[32]`

- `struct calib_coeffs_t`
  - (packed)
  - `uint8_t response_type`
  - `float coeff3`
  - `float coeff2`
  - `float coeff1`
  - `float coeff0`
  - `uint8_t request_id`

- `struct sg_float_ota_status_t`
  - (packed)
  - `uint8_t packet_type`
  - `uint8_t version`
  - `uint16_t sequence_id`
  - `uint32_t uptime_s`
  - `char ssid[28]`
  - `char ip[16]`
  - `uint16_t timeout_s`
  - `uint16_t crc`

- `struct sg_plug_command_t`
  - (packed)
  - `uint8_t packet_type`
  - `uint8_t version`
  - `uint16_t sequence_id`
  - `uint32_t base_epoch`
  - `float beer_target_c`
  - `float ramp_k_per_h`
  - `float batch_liters`
  - `float controller_kp`
  - `float controller_tn_h`
  - `float controller_d_brake_h`
  - `float air_turn_off_above_target_c`
  - `float air_turn_on_above_target_c`
  - `uint16_t air_minimum_on_s`
  - `uint16_t air_minimum_off_s`
  - `float cold_integral_band_c`
  - `float warm_integral_band_c`
  - `float max_positive_integral_c`
  - `float max_negative_integral_c`
  - `float integral_leak_per_hour`
  - `float error_crossing_keep_factor`
  - `float max_d_offset_c`
  - `float warming_d_factor`
  - `float beer_undershoot_lockout_c`
  - `float fast_warming_rate_k_per_h`
  - `float strong_undershoot_c`
  - `float strong_undershoot_air_offset_c`
  - `float min_air_target_c`
  - `float max_air_target_c`
  - `float target_step_c`
  - `float ramp_controller_kp_h`
  - `float ramp_controller_tn_h`
  - `float max_ramp_trim_c`
  - `float ramp_fade_distance_c`
  - `float rate_filter_samples`
  - `uint8_t flags`
  - `uint16_t crc`

- `struct sg_plug_status_t`
  - (packed)
  - `uint8_t packet_type`
  - `uint8_t version`
  - `uint16_t sequence_id`
  - `uint16_t command_sequence_id`
  - `uint32_t uptime_s`
  - `float air_temp_c`
  - `float beer_temp_c`
  - `float beer_target_c`
  - `float air_target_c`
  - `float duty_10m_percent`
  - `float pi_offset_c`
  - `float pi_tn_hours`
  - `float controller_kp`
  - `float controller_d_brake_h`
  - `float beer_rate_c_per_h`
  - `float ramp_k_per_h`
  - `uint8_t control_mode`
  - `uint16_t faults`
  - `uint8_t relay_on`
  - `uint8_t pattern_ready`
  - `uint16_t crc`

- `struct sg_plug_status_v2_t`
  - (packed)
  - `uint8_t packet_type`
  - `uint8_t version`
  - `uint16_t sequence_id`
  - `uint16_t command_sequence_id`
  - `uint32_t uptime_s`
  - `float air_temp_c`
  - `float beer_temp_c`
  - `float beer_target_c`
  - `float air_target_c`
  - `float duty_10m_percent`
  - `float pi_offset_c`
  - `float pi_tn_hours`
  - `float controller_kp`
  - `float controller_d_brake_h`
  - `float beer_rate_c_per_h`
  - `float ramp_k_per_h`
  - `uint8_t control_mode`
  - `uint16_t faults`
  - `uint8_t relay_on`
  - `uint8_t pattern_ready`
  - `float p_offset_c`
  - `float i_offset_c`
  - `float d_offset_c`
  - `float ramp_trim_c`
  - `float ramp_i_offset_c`
  - `float ramp_rate_error_k_per_h`
  - `float beer_rate_raw_c_per_h`
  - `uint16_t crc`

### Functions

- `void sg_crc16 (const uint8_t* data, size_t length) const`

### Macros

- `#define SG_PROTOCOL_VERSION 2`

- `#define SG_PLUG_COMMAND_TYPE 0xC1`

- `#define SG_PLUG_STATUS_TYPE 0xC2`

- `#define SG_FLOAT_OTA_STATUS_TYPE 0xF1`

- `#define SG_PLUG_COMMAND_ENABLE 0x01`

- `#define SG_PLUG_FAULT_AIR_SENSOR 0x0001`

- `#define SG_PLUG_FAULT_BEER_SENSOR 0x0002`

- `#define SG_PLUG_FAULT_NO_PATTERN 0x0004`

- `#define SG_PLUG_FAULT_COMMAND_TIMEOUT 0x0008`

- `#define SG_PAYLOAD_FLAG_DELAYED 0x01`

- `#define SG_PAYLOAD_FLAG_SENSOR_ERR 0x02`

- `#define SG_PAYLOAD_FLAG_LOW_BATT 0x04`

- `#define SG_PAYLOAD_FLAG_ZERO_CAL_OK 0x08`

- `#define SG_PAYLOAD_FLAG_ZERO_CAL_FAIL 0x10`

- `#define SG_ACK_COMMAND_NONE 0`

- `#define SG_ACK_COMMAND_ZERO_CALIBRATE 1`

### Type Aliases

- `using version = struct __attribute__((packed)) { uint8_t`

- `using command = struct __attribute__((packed)) { uint8_t`

- `using packet_type = struct __attribute__((packed)) { uint8_t`

- `using response_type = struct __attribute__((packed)) { uint8_t`

- `using response_type = struct __attribute__((packed)) { uint8_t`

- `using packet_type = struct __attribute__((packed)) { uint8_t`

- `using packet_type = struct __attribute__((packed)) { uint8_t`

- `using packet_type = struct __attribute__((packed)) { uint8_t`

- `using packet_type = struct __attribute__((packed)) { uint8_t`

## Key Dependencies

**System/External:**
- `<stdint.h>`
- `<stddef.h>`

## Side Effects & Hardware Access

- Sensor reading
- Temperature target calculation
- Calibration coefficient computation
- Execution delay

## Change Risks

- **HIGH**: Protocol changes break compatibility

---
*Last modified (hash): 4a349e41c279633b6493caa3db926265a8265f92a8558487be1d6c10f8ca56f1*