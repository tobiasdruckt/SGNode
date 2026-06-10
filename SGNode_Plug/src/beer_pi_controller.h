#pragma once

#include <Arduino.h>

class BeerPiController {
 public:
  static constexpr uint8_t AVERAGE_SAMPLES = 60;
  static constexpr float KP = 2.0f;
  static constexpr float DEADBAND_C = 0.1f;

  void addBeerSample(float beerC, bool valid);
  float update(float beerTargetC, float rampKPerHour, float batchLiters, uint32_t nowMs);
  void reset();

  float beerAverageC() const;
  float airTargetC() const;
  float offsetC() const;
  float tnHours() const;
  static float calculateTnHours(float batchLiters);

 private:
  float samples_[AVERAGE_SAMPLES] = {};
  uint8_t next_ = 0;
  uint8_t count_ = 0;
  float sum_ = 0.0f;
  float integral_ = 0.0f;
  float airTargetC_ = NAN;
  float offsetC_ = 0.0f;
  float tnHours_ = 10.0f;
  uint32_t lastUpdateMs_ = 0;
};
