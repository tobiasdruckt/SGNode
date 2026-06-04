#include "eta_predictor.h"

ETAResult ETAPredictor::predict(float currentSG, float expectedFG, float gravityDeltaPerHour) {
  ETAResult result;
  result.secondsToFG = 0;
  result.secondsToPackaging = 0;
  result.valid = false;
  result.presetBased = false;
  result.confidencePercent = 0;

  float dropRate = gravityDeltaPerHour < 0.0f ? -gravityDeltaPerHour : 0.0f;
  if (currentSG <= expectedFG + 0.001f) {
    result.valid = true;
    result.secondsToFG = 0;
    result.secondsToPackaging = 48UL * 3600UL;
    result.confidencePercent = 80;
    return result;
  }

  if (dropRate < 0.00002f) return result;

  float remaining = currentSG - expectedFG;
  unsigned long hours = (unsigned long)(remaining / dropRate);
  if (hours > 24UL * 21UL) hours = 24UL * 21UL;
  result.valid = true;
  result.secondsToFG = hours * 3600UL;
  result.secondsToPackaging = result.secondsToFG + 48UL * 3600UL;
  result.confidencePercent = 70;
  return result;
}

ETAResult ETAPredictor::predict(const BrewProfile& profile, float currentSG, float expectedFG,
                                float gravityDeltaPerHour, unsigned long elapsedSeconds) {
  ETAResult trend = predict(currentSG, expectedFG, gravityDeltaPerHour);
  if (!profile.autoModeEnabled || profile.typicalDurationHours <= 0.0f) return trend;

  unsigned long presetTotal = (unsigned long)(profile.typicalDurationHours * 3600.0f);
  unsigned long presetRemaining = elapsedSeconds >= presetTotal ? 0 : presetTotal - elapsedSeconds;
  ETAResult result = trend;

  if (!trend.valid) {
    result.valid = true;
    result.presetBased = true;
    result.secondsToFG = presetRemaining;
    result.secondsToPackaging = presetRemaining + 48UL * 3600UL;
    result.confidencePercent = elapsedSeconds < 12UL * 3600UL ? 35 : 50;
    return result;
  }

  unsigned long blended = (trend.secondsToFG * 2UL + presetRemaining) / 3UL;
  if (elapsedSeconds < 24UL * 3600UL) {
    blended = (trend.secondsToFG + presetRemaining * 2UL) / 3UL;
    result.presetBased = true;
    result.confidencePercent = 55;
  } else {
    result.presetBased = false;
    result.confidencePercent = 75;
  }
  result.secondsToFG = blended;
  result.secondsToPackaging = blended + 48UL * 3600UL;
  return result;
}
