#include "beer_pi_controller.h"

namespace {

constexpr uint32_t PI_INTERVAL_MS = 60000;
constexpr float MIN_RATE_FOR_D_C_PER_H = 0.05f;

float clampValue(float value, float minimum, float maximum) {
  return value < minimum ? minimum : (value > maximum ? maximum : value);
}

float sanitizeOrDefault(float value, float fallback, float minimum, float maximum) {
  if (!isfinite(value)) return fallback;
  return clampValue(value, minimum, maximum);
}

BeerPiSettings sanitizeSettings(const BeerPiSettings& settings) {
  BeerPiSettings sanitized;
  sanitized.kp = sanitizeOrDefault(settings.kp, BeerPiController::DEFAULT_KP, 0.05f, 5.0f);
  sanitized.integralTnHours = sanitizeOrDefault(settings.integralTnHours, 0.75f, 0.0f, 72.0f);
  sanitized.dBrakeHours = sanitizeOrDefault(settings.dBrakeHours,
                                            BeerPiController::DEFAULT_D_BRAKE_HOURS,
                                            0.0f, 6.0f);
  sanitized.coldIntegralBandC = sanitizeOrDefault(settings.coldIntegralBandC, 1.0f, 0.0f, 5.0f);
  sanitized.warmIntegralBandC = sanitizeOrDefault(settings.warmIntegralBandC, 0.5f, 0.0f, 5.0f);
  sanitized.maxPositiveIntegralC = sanitizeOrDefault(settings.maxPositiveIntegralC, 2.0f, 0.0f, 5.0f);
  sanitized.maxNegativeIntegralC = sanitizeOrDefault(settings.maxNegativeIntegralC, -0.6f, -5.0f, 0.0f);
  sanitized.integralLeakPerHour = sanitizeOrDefault(settings.integralLeakPerHour, 1.0f, 0.0f, 5.0f);
  sanitized.errorCrossingKeepFactor = sanitizeOrDefault(settings.errorCrossingKeepFactor, 0.25f, 0.0f, 1.0f);
  sanitized.maxDOffsetC = sanitizeOrDefault(settings.maxDOffsetC, 0.9f, 0.0f, 5.0f);
  sanitized.warmingDFactor = sanitizeOrDefault(settings.warmingDFactor, 0.25f, 0.0f, 1.0f);
  sanitized.strongUndershootC = sanitizeOrDefault(settings.strongUndershootC, 1.0f, 0.0f, 5.0f);
  sanitized.strongUndershootAirOffsetC = sanitizeOrDefault(settings.strongUndershootAirOffsetC, 1.8f, 0.0f, 5.0f);
  sanitized.minAirTargetC = sanitizeOrDefault(settings.minAirTargetC, 1.0f, -5.0f, 20.0f);
  sanitized.maxAirTargetC = sanitizeOrDefault(settings.maxAirTargetC, 30.0f, 0.0f, 35.0f);
  if (sanitized.maxAirTargetC <= sanitized.minAirTargetC + 0.5f) {
    sanitized.maxAirTargetC = sanitized.minAirTargetC + 0.5f;
  }
  sanitized.targetStepC = sanitizeOrDefault(settings.targetStepC, 0.25f, 0.0f, 5.0f);
  sanitized.rampControllerKpHours = sanitizeOrDefault(settings.rampControllerKpHours, 0.8f, 0.0f, 6.0f);
  sanitized.rampControllerTnHours = sanitizeOrDefault(settings.rampControllerTnHours, 3.0f, 0.0f, 24.0f);
  sanitized.maxRampTrimC = sanitizeOrDefault(settings.maxRampTrimC, 1.0f, 0.0f, 3.0f);
  sanitized.rampFadeDistanceC = sanitizeOrDefault(settings.rampFadeDistanceC, 1.0f, 0.0f, 5.0f);
  sanitized.rateFilterSamples = sanitizeOrDefault(settings.rateFilterSamples, 5.0f, 1.0f, 20.0f);
  return sanitized;
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
  sampleTimesMs_[next_] = millis();
  sum_ += beerC;
  next_ = (next_ + 1) % AVERAGE_SAMPLES;
}

float BeerPiController::update(float beerTargetC, float rampKPerHour,
                               float batchLiters, const BeerPiSettings& settings,
                               uint32_t nowMs) {
  if (count_ == 0 || !isfinite(beerTargetC)) {
    return airTargetC_;
  }
  if (lastUpdateMs_ != 0 && nowMs - lastUpdateMs_ < PI_INTERVAL_MS) {
    return airTargetC_;
  }

  const float dtHours = lastUpdateMs_ == 0 ? 0.0f :
                        (nowMs - lastUpdateMs_) / 3600000.0f;
  lastUpdateMs_ = nowMs;
  const BeerPiSettings sanitized = sanitizeSettings(settings);
  if (isfinite(lastBeerTargetC_) &&
      fabsf(beerTargetC - lastBeerTargetC_) > sanitized.targetStepC) {
    integral_ = 0.0f;
    rampIntegralC_ = 0.0f;
    lastErrorC_ = NAN;
  }
  lastBeerTargetC_ = beerTargetC;

  kp_ = sanitized.kp;
  dBrakeHours_ = sanitized.dBrakeHours;
  (void)batchLiters;

  beerRateRawCPerHour_ = calculateBeerRateCPerHour();
  if (!beerRateFilterPrimed_ || !isfinite(beerRateCPerHour_)) {
    beerRateCPerHour_ = beerRateRawCPerHour_;
    beerRateFilterPrimed_ = true;
  } else {
    const float alpha = 1.0f / sanitized.rateFilterSamples;
    beerRateCPerHour_ += alpha * (beerRateRawCPerHour_ - beerRateCPerHour_);
  }
  float errorC = beerTargetC - beerControlC();
  if (fabsf(errorC) <= DEADBAND_C) {
    errorC = 0.0f;
  }
  const bool integralActive =
    sanitized.integralTnHours > 0.0f &&
    ((errorC >= 0.0f && errorC <= sanitized.coldIntegralBandC) ||
     (errorC < 0.0f && fabsf(errorC) <= sanitized.warmIntegralBandC));
  tnHours_ = integralActive ? sanitized.integralTnHours : 0.0f;

  float dBrakeOffsetC = 0.0f;
  if (fabsf(beerRateCPerHour_) >= MIN_RATE_FOR_D_C_PER_H) {
    const float effectiveDHours = beerRateCPerHour_ > 0.0f
      ? dBrakeHours_ * sanitized.warmingDFactor
      : dBrakeHours_;
    dBrakeOffsetC = -beerRateCPerHour_ * effectiveDHours;
    dBrakeOffsetC = clampValue(dBrakeOffsetC, -sanitized.maxDOffsetC, sanitized.maxDOffsetC);
  }
  if (integralActive && isfinite(lastErrorC_) &&
      lastErrorC_ * errorC < 0.0f) {
    integral_ *= sanitized.errorCrossingKeepFactor;
  }
  float candidateIntegral = integral_;
  if (integralActive && dtHours > 0.0f) {
    candidateIntegral += (kp_ / sanitized.integralTnHours) * errorC * dtHours;
  } else if (!integralActive && dtHours > 0.0f) {
    const float leakFactor = clampValue(1.0f - sanitized.integralLeakPerHour * dtHours, 0.0f, 1.0f);
    candidateIntegral *= leakFactor;
  }
  candidateIntegral = clampValue(candidateIntegral, sanitized.maxNegativeIntegralC, sanitized.maxPositiveIntegralC);
  rampTrimC_ = 0.0f;
  rampRateErrorKPerHour_ = 0.0f;
  const float distanceToTargetC = fabsf(beerTargetC - beerControlC());
  const bool rampControllerActive = isfinite(rampKPerHour) &&
    rampKPerHour > 0.01f &&
    sanitized.maxRampTrimC > 0.0f &&
    distanceToTargetC > sanitized.rampFadeDistanceC;
  if (rampControllerActive) {
    const float desiredRateKPerHour = beerTargetC >= beerControlC()
      ? fabsf(rampKPerHour)
      : -fabsf(rampKPerHour);
    rampRateErrorKPerHour_ = desiredRateKPerHour - beerRateCPerHour_;
    float fade = 1.0f;
    if (sanitized.rampFadeDistanceC > 0.01f &&
        distanceToTargetC < sanitized.rampFadeDistanceC * 2.0f) {
      fade = (distanceToTargetC - sanitized.rampFadeDistanceC) / sanitized.rampFadeDistanceC;
      fade = clampValue(fade, 0.0f, 1.0f);
    }
    float candidateRampIntegral = rampIntegralC_;
    if (sanitized.rampControllerTnHours > 0.0f && dtHours > 0.0f) {
      candidateRampIntegral += (sanitized.rampControllerKpHours / sanitized.rampControllerTnHours) *
                               rampRateErrorKPerHour_ * dtHours;
    }
    candidateRampIntegral = clampValue(candidateRampIntegral,
                                       -sanitized.maxRampTrimC,
                                       sanitized.maxRampTrimC);
    const float rampPTrimC = sanitized.rampControllerKpHours * rampRateErrorKPerHour_;
    rampTrimC_ = clampValue((rampPTrimC + candidateRampIntegral) * fade,
                            -sanitized.maxRampTrimC,
                            sanitized.maxRampTrimC);
    rampIntegralC_ = candidateRampIntegral;
  } else if (dtHours > 0.0f) {
    const float leakFactor = clampValue(1.0f - dtHours, 0.0f, 1.0f);
    rampIntegralC_ *= leakFactor;
  }
  pOffsetC_ = kp_ * errorC;
  iOffsetC_ = candidateIntegral;
  dOffsetC_ = dBrakeOffsetC;
  const float rawOffsetC = pOffsetC_ + iOffsetC_ + dOffsetC_ + rampTrimC_;
  float rawAirTargetC = beerTargetC + rawOffsetC;
  if (errorC > sanitized.strongUndershootC) {
    rawAirTargetC = max(rawAirTargetC, beerTargetC + sanitized.strongUndershootAirOffsetC);
  }
  airTargetC_ = clampValue(rawAirTargetC, sanitized.minAirTargetC, sanitized.maxAirTargetC);
  offsetC_ = airTargetC_ - beerTargetC;

  const bool saturatedLow = rawAirTargetC < sanitized.minAirTargetC;
  const bool saturatedHigh = rawAirTargetC > sanitized.maxAirTargetC;
  const bool integratingFurtherIntoLow = saturatedLow && errorC < 0.0f;
  const bool integratingFurtherIntoHigh = saturatedHigh && errorC > 0.0f;
  if (!integralActive || (!integratingFurtherIntoLow && !integratingFurtherIntoHigh)) {
    integral_ = candidateIntegral;
  }
  integral_ = clampValue(integral_, sanitized.maxNegativeIntegralC, sanitized.maxPositiveIntegralC);
  lastErrorC_ = errorC;
  return airTargetC_;
}

void BeerPiController::reset() {
  memset(samples_, 0, sizeof(samples_));
  memset(sampleTimesMs_, 0, sizeof(sampleTimesMs_));
  next_ = 0;
  count_ = 0;
  sum_ = 0.0f;
  integral_ = 0.0f;
  airTargetC_ = NAN;
  offsetC_ = 0.0f;
  pOffsetC_ = 0.0f;
  iOffsetC_ = 0.0f;
  dOffsetC_ = 0.0f;
  rampIntegralC_ = 0.0f;
  rampTrimC_ = 0.0f;
  rampRateErrorKPerHour_ = 0.0f;
  beerRateRawCPerHour_ = 0.0f;
  beerRateCPerHour_ = 0.0f;
  beerRateFilterPrimed_ = false;
  lastBeerTargetC_ = NAN;
  lastErrorC_ = NAN;
  lastUpdateMs_ = 0;
}

float BeerPiController::beerAverageC() const {
  return count_ == 0 ? NAN : sum_ / count_;
}

float BeerPiController::beerControlC() const {
  if (count_ == 0) return NAN;
  const uint8_t samplesToUse = count_ < CONTROL_AVERAGE_SAMPLES ? count_ : CONTROL_AVERAGE_SAMPLES;
  float total = 0.0f;
  for (uint8_t i = 0; i < samplesToUse; ++i) {
    uint8_t idx = (next_ + AVERAGE_SAMPLES - 1 - i) % AVERAGE_SAMPLES;
    total += samples_[idx];
  }
  return total / samplesToUse;
}

float BeerPiController::beerRateCPerHour() const {
  return beerRateCPerHour_;
}

float BeerPiController::beerRateRawCPerHour() const {
  return beerRateRawCPerHour_;
}

float BeerPiController::calculateBeerRateCPerHour() const {
  if (count_ < 2) return 0.0f;
  const uint8_t newestIdx = (next_ + AVERAGE_SAMPLES - 1) % AVERAGE_SAMPLES;
  const uint8_t oldestIdx = count_ == AVERAGE_SAMPLES ? next_ : 0;
  const uint32_t newestMs = sampleTimesMs_[newestIdx];
  const uint32_t oldestMs = sampleTimesMs_[oldestIdx];
  if (newestMs <= oldestMs) return 0.0f;
  const float dtHours = (newestMs - oldestMs) / 3600000.0f;
  if (dtHours <= 0.0f) return 0.0f;
  return (samples_[newestIdx] - samples_[oldestIdx]) / dtHours;
}

float BeerPiController::airTargetC() const {
  return airTargetC_;
}

float BeerPiController::offsetC() const {
  return offsetC_;
}

float BeerPiController::pOffsetC() const {
  return pOffsetC_;
}

float BeerPiController::iOffsetC() const {
  return iOffsetC_;
}

float BeerPiController::dOffsetC() const {
  return dOffsetC_;
}

float BeerPiController::rampTrimC() const {
  return rampTrimC_;
}

float BeerPiController::rampITrimC() const {
  return rampIntegralC_;
}

float BeerPiController::rampRateErrorKPerHour() const {
  return rampRateErrorKPerHour_;
}

float BeerPiController::tnHours() const {
  return tnHours_;
}

float BeerPiController::kp() const {
  return kp_;
}

float BeerPiController::dBrakeHours() const {
  return dBrakeHours_;
}

float BeerPiController::calculateTnHours(float batchLiters) {
  return clampValue(10.0f * batchLiters / 13.0f, 6.0f, 24.0f);
}
