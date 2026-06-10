#include "beer_pi_controller.h"

namespace {

constexpr uint32_t PI_INTERVAL_MS = 60000;
constexpr float HOLD_MAX_OFFSET_C = 3.0f;
constexpr float RAMP_MAX_OFFSET_C = 5.0f;
constexpr float FAST_RAMP_THRESHOLD_K_PER_H = 0.8f;
constexpr float FAST_RAMP_MIN_AIR_TARGET_C = 1.0f;

float clampValue(float value, float minimum, float maximum) {
  return value < minimum ? minimum : (value > maximum ? maximum : value);
}

}  // namespace

void BeerPiController::addBeerSample(float beerC, bool valid) {
  if (!valid || !isfinite(beerC)) {
    return;
  }

  if (count_ == AVERAGE_SAMPLES) {
    sum_ -= samples_[next_];
  } else {
    ++count_;
  }
  samples_[next_] = beerC;
  sum_ += beerC;
  next_ = (next_ + 1) % AVERAGE_SAMPLES;
}

float BeerPiController::update(float beerTargetC, float rampKPerHour,
                               float batchLiters, uint32_t nowMs) {
  if (count_ == 0 || !isfinite(beerTargetC)) {
    return airTargetC_;
  }
  if (lastUpdateMs_ != 0 && nowMs - lastUpdateMs_ < PI_INTERVAL_MS) {
    return airTargetC_;
  }

  const float dtHours = lastUpdateMs_ == 0 ? 0.0f :
                        (nowMs - lastUpdateMs_) / 3600000.0f;
  lastUpdateMs_ = nowMs;
  tnHours_ = calculateTnHours(batchLiters);

  float errorC = beerTargetC - beerAverageC();
  if (fabsf(errorC) <= DEADBAND_C) {
    errorC = 0.0f;
  }

  const float maxOffsetC = fabsf(rampKPerHour) > 0.01f ?
                           RAMP_MAX_OFFSET_C : HOLD_MAX_OFFSET_C;
  const float candidateIntegral = integral_ + (KP / tnHours_) * errorC * dtHours;
  const float unclampedOffset = KP * errorC + candidateIntegral;
  offsetC_ = clampValue(unclampedOffset, -maxOffsetC, 0.0f);

  // Back-calculate the integrator to the achievable output for anti-windup.
  integral_ = clampValue(offsetC_ - KP * errorC, -maxOffsetC, maxOffsetC);
  airTargetC_ = beerTargetC + offsetC_;
  if (fabsf(rampKPerHour) > FAST_RAMP_THRESHOLD_K_PER_H &&
      airTargetC_ < FAST_RAMP_MIN_AIR_TARGET_C) {
    airTargetC_ = FAST_RAMP_MIN_AIR_TARGET_C;
    offsetC_ = airTargetC_ - beerTargetC;
  }
  return airTargetC_;
}

void BeerPiController::reset() {
  memset(samples_, 0, sizeof(samples_));
  next_ = 0;
  count_ = 0;
  sum_ = 0.0f;
  integral_ = 0.0f;
  airTargetC_ = NAN;
  offsetC_ = 0.0f;
  lastUpdateMs_ = 0;
}

float BeerPiController::beerAverageC() const {
  return count_ == 0 ? NAN : sum_ / count_;
}

float BeerPiController::airTargetC() const {
  return airTargetC_;
}

float BeerPiController::offsetC() const {
  return offsetC_;
}

float BeerPiController::tnHours() const {
  return tnHours_;
}

float BeerPiController::calculateTnHours(float batchLiters) {
  return clampValue(10.0f * batchLiters / 13.0f, 6.0f, 24.0f);
}
