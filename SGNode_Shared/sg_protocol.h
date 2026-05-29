#pragma once

#include <stdint.h>

#define SG_PROTOCOL_VERSION 2

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

// Compile-time size validation
static_assert(sizeof(payload_t) == 26, "payload_t size mismatch");
static_assert(sizeof(calib_command_t) == 6, "calib_command_t size mismatch");
static_assert(sizeof(ack_packet_t) == 5, "ack_packet_t size mismatch");
static_assert(sizeof(calib_response_t) == 42, "calib_response_t size mismatch");
static_assert(sizeof(calib_coeffs_t) == 26, "calib_coeffs_t size mismatch");
