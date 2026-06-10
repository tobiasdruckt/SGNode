#pragma once

#include <stdint.h>
#include <stddef.h>

#define SG_PROTOCOL_VERSION 2

#define SG_PLUG_COMMAND_TYPE 0xC1
#define SG_PLUG_STATUS_TYPE 0xC2
#define SG_PLUG_COMMAND_ENABLE 0x01
#define SG_PLUG_FAULT_AIR_SENSOR 0x0001
#define SG_PLUG_FAULT_BEER_SENSOR 0x0002
#define SG_PLUG_FAULT_NO_PATTERN 0x0004
#define SG_PLUG_FAULT_COMMAND_TIMEOUT 0x0008

#define SG_PAYLOAD_FLAG_DELAYED 0x01
#define SG_PAYLOAD_FLAG_SENSOR_ERR 0x02
#define SG_PAYLOAD_FLAG_LOW_BATT 0x04
#define SG_PAYLOAD_FLAG_ZERO_CAL_OK 0x08
#define SG_PAYLOAD_FLAG_ZERO_CAL_FAIL 0x10

#define SG_ACK_COMMAND_NONE 0
#define SG_ACK_COMMAND_ZERO_CALIBRATE 1

// Data structure for transmission
// Size: 1 + 2 + 4 + 4 + 4 + 4 + 4 + 1 + 2 = 26 bytes
typedef struct __attribute__((packed)) {
  uint8_t version;        // Protocol version (current: 2)
  uint16_t sequence_id;   // Rolling sequence number
  uint32_t uptime_s;      // Uptime in seconds
  float angle;
  float density;
  float temperature;
  float battery_voltage;  // Battery voltage in volts
  uint8_t flags;          // Bitfield: bit0=delayed, bit1=sensor_err, bit2=low_batt
  uint16_t crc;           // CRC16 checksum
} payload_t;

// Calibration command structure
// Size: 1 + 4 + 1 = 6 bytes
typedef struct __attribute__((packed)) {
  uint8_t command;        // 0-3=legacy points, 4=APPLY, 5=OFFSET, 6=TRIGGER, 7=EXIT, 8-13=variable SG points
  float target_sg;        // Target specific gravity for calibration point
  uint8_t request_id;     // Unique ID for response tracking
} calib_command_t;

typedef struct __attribute__((packed)) {
  uint8_t packet_type;     // 0xA5 = payload ACK
  uint16_t sequence_id;    // Echoed payload sequence
  uint16_t highest_seen;   // Highest sequence observed by base in this boot/session
  uint8_t command;         // SG_ACK_COMMAND_*
  uint8_t command_id;      // Monotonic command marker from base
} ack_packet_t;

// Calibration response structure
// Size: 1 + 4 + 4 + 1 + 32 = 42 bytes
typedef struct __attribute__((packed)) {
  uint8_t response_type;  // 0=CALIBRATION_DATA, 1=ACK, 2=ERROR
  float angle;           // Measured angle
  float sg;              // Calculated SG
  uint8_t request_id;    // Echo back request ID
  char message[32];       // Status message
} calib_response_t;

// Calibration coefficients structure
// Size: 1 + 4 + 4 + 4 + 4 + 4 + 4 + 1 = 26 bytes
typedef struct __attribute__((packed)) {
  uint8_t response_type;  // 3=COEFFICIENTS
  float coeff3;          // Cubic coefficient
  float coeff2;          // Quadratic coefficient
  float coeff1;          // Linear coefficient
  float coeff0;          // Constant term
  float norm_offset;     // Normalization offset (Tilt - offset)
  float norm_scale;      // Normalization scale (divide by scale)
  uint8_t request_id;    // Echo back request ID
} calib_coeffs_t;

typedef struct __attribute__((packed)) {
  uint8_t packet_type;
  uint8_t version;
  uint16_t sequence_id;
  uint32_t base_epoch;
  float beer_target_c;
  float ramp_k_per_h;
  float batch_liters;
  uint8_t flags;
  uint16_t crc;
} sg_plug_command_t;

typedef struct __attribute__((packed)) {
  uint8_t packet_type;
  uint8_t version;
  uint16_t sequence_id;
  uint16_t command_sequence_id;
  uint32_t uptime_s;
  float air_temp_c;
  float beer_temp_c;
  float beer_target_c;
  float air_target_c;
  float duty_10m_percent;
  float pi_offset_c;
  float pi_tn_hours;
  uint8_t control_mode;
  uint16_t faults;
  uint8_t relay_on;
  uint8_t pattern_ready;
  uint16_t crc;
} sg_plug_status_t;

static inline uint16_t sg_crc16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (crc & 1 ? 0xA001 : 0);
    }
  }
  return crc;
}

// Compile-time size validation
static_assert(sizeof(payload_t) == 26, "payload_t size mismatch");
static_assert(sizeof(calib_command_t) == 6, "calib_command_t size mismatch");
static_assert(sizeof(ack_packet_t) == 7, "ack_packet_t size mismatch");
static_assert(sizeof(calib_response_t) == 42, "calib_response_t size mismatch");
static_assert(sizeof(calib_coeffs_t) == 26, "calib_coeffs_t size mismatch");
static_assert(sizeof(sg_plug_command_t) == 23, "sg_plug_command_t size mismatch");
static_assert(sizeof(sg_plug_status_t) == 45, "sg_plug_status_t size mismatch");
