#include "air_controller.h"

AirController::AirController(const AirControllerConfig& config) : config_(config) {}

void AirController::setConfig(const AirControllerConfig& config) {
  config_ = config;
}

bool AirController::update(float airC, bool airValid, float targetC, bool enabled,
                           uint32_t nowMs) {
  if (!enabled || !airValid || !isfinite(targetC)) {
    if (relayOn_) {
      relayOn_ = false;
      lastChangeMs_ = nowMs;
    }
    return relayOn_;
  }

  const uint32_t elapsedMs = nowMs - lastChangeMs_;
  const float turnOnC = targetC + config_.turnOnAboveTargetC;
  const float turnOffC = targetC + config_.turnOffAboveTargetC;

  if (!relayOn_ && airC > turnOnC && elapsedMs >= config_.minimumOffMs) {
    relayOn_ = true;
    lastChangeMs_ = nowMs;
  } else if (relayOn_ && airC < turnOffC &&
             elapsedMs >= config_.minimumOnMs) {
    relayOn_ = false;
    lastChangeMs_ = nowMs;
  }

  return relayOn_;
}

bool AirController::relayOn() const {
  return relayOn_;
}

uint32_t AirController::lastChangeMs() const {
  return lastChangeMs_;
}
