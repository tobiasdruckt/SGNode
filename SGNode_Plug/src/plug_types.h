#pragma once

#include <Arduino.h>
#include <sg_protocol.h>

enum class PlugControlMode : uint8_t {
  OFF = 0,
  AUTOMATIC = 1,
  AIR_ONLY_HOLD = 2,
  TIME_PATTERN_FALLBACK = 3,
  FAULT = 4,
};

enum PlugFault : uint16_t {
  PLUG_FAULT_NONE = 0,
  PLUG_FAULT_AIR_SENSOR = SG_PLUG_FAULT_AIR_SENSOR,
  PLUG_FAULT_BEER_SENSOR = SG_PLUG_FAULT_BEER_SENSOR,
  PLUG_FAULT_NO_PATTERN = SG_PLUG_FAULT_NO_PATTERN,
  PLUG_FAULT_COMMAND_TIMEOUT = SG_PLUG_FAULT_COMMAND_TIMEOUT,
};

struct SensorSnapshot {
  float airC = NAN;
  float beerC = NAN;
  bool airValid = false;
  bool beerValid = false;
  uint32_t sampledAtMs = 0;
};
