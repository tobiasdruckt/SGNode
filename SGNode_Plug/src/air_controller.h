#pragma once

#include <Arduino.h>

struct AirControllerConfig {
  float turnOffAboveTargetC = 0.5f;
  float turnOnAboveTargetC = 1.1f;
  uint32_t minimumOnMs = 120000;
  uint32_t minimumOffMs = 300000;
};

class AirController {
 public:
  explicit AirController(const AirControllerConfig& config = AirControllerConfig());

  bool update(float airC, bool airValid, float targetC, bool enabled, uint32_t nowMs);
  void setConfig(const AirControllerConfig& config);
  bool relayOn() const;
  uint32_t lastChangeMs() const;

 private:
  AirControllerConfig config_;
  bool relayOn_ = false;
  uint32_t lastChangeMs_ = 0;
};
