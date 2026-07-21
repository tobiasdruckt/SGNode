#pragma once

#include <Arduino.h>

struct BeerPiSettings {
  float kp = 0.45f;
  float integralTnHours = 0.75f;
  float dBrakeHours = 0.8f;
  float coldIntegralBandC = 1.0f;
  float warmIntegralBandC = 0.5f;
  float maxPositiveIntegralC = 2.0f;
  float maxNegativeIntegralC = -0.6f;
  float integralLeakPerHour = 1.0f;
  float errorCrossingKeepFactor = 0.25f;
  float maxDOffsetC = 0.9f;
  float warmingDFactor = 0.25f;
  float strongUndershootC = 1.0f;
  float strongUndershootAirOffsetC = 1.8f;
  float minAirTargetC = 1.0f;
  float maxAirTargetC = 30.0f;
  float targetStepC = 0.25f;
  float rampControllerKpHours = 0.8f;
  float rampControllerTnHours = 3.0f;
  float maxRampTrimC = 1.0f;
  float rampFadeDistanceC = 1.0f;
  float rateFilterSamples = 5.0f;
};

class BeerPiController {
 public:
  static constexpr uint8_t AVERAGE_SAMPLES = 60;
  static constexpr uint8_t CONTROL_AVERAGE_SAMPLES = 6;
  static constexpr float DEFAULT_KP = 0.45f;
  static constexpr float DEFAULT_D_BRAKE_HOURS = 0.8f;
  static constexpr float DEADBAND_C = 0.1f;

  void addBeerSample(float beerC, bool valid);
  float update(float beerTargetC, float rampKPerHour, float batchLiters,
               const BeerPiSettings& settings, uint32_t nowMs);
  void reset();

  float beerAverageC() const;
  float beerControlC() const;
  float beerRateCPerHour() const;
  float beerRateRawCPerHour() const;
  float airTargetC() const;
  float offsetC() const;
  float pOffsetC() const;
  float iOffsetC() const;
  float dOffsetC() const;
  float rampTrimC() const;
  float rampITrimC() const;
  float rampRateErrorKPerHour() const;
  float tnHours() const;
  float kp() const;
  float dBrakeHours() const;
  static float calculateTnHours(float batchLiters);

 private:
  float samples_[AVERAGE_SAMPLES] = {};
  uint32_t sampleTimesMs_[AVERAGE_SAMPLES] = {};
  uint8_t next_ = 0;
  uint8_t count_ = 0;
  float sum_ = 0.0f;
  float integral_ = 0.0f;
  float airTargetC_ = NAN;
  float offsetC_ = 0.0f;
  float pOffsetC_ = 0.0f;
  float iOffsetC_ = 0.0f;
  float dOffsetC_ = 0.0f;
  float rampIntegralC_ = 0.0f;
  float rampTrimC_ = 0.0f;
  float rampRateErrorKPerHour_ = 0.0f;
  float beerRateRawCPerHour_ = 0.0f;
  float tnHours_ = 0.75f;
  float kp_ = DEFAULT_KP;
  float dBrakeHours_ = DEFAULT_D_BRAKE_HOURS;
  float beerRateCPerHour_ = 0.0f;
  bool beerRateFilterPrimed_ = false;
  float lastBeerTargetC_ = NAN;
  float lastErrorC_ = NAN;
  uint32_t lastUpdateMs_ = 0;

  float calculateBeerRateCPerHour() const;
};
