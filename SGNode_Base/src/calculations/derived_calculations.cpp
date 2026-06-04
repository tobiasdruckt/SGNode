#include "derived_calculations.h"

float DerivedCalculations::expectedFG(float og, int apparentAttenuationPercent) {
  if (og <= 1.0f || apparentAttenuationPercent <= 0) return 1.000f;
  float points = (og - 1.0f) * 1000.0f;
  float remaining = points * (1.0f - apparentAttenuationPercent / 100.0f);
  return 1.0f + remaining / 1000.0f;
}

float DerivedCalculations::apparentAttenuation(float effectiveOG, float currentSG) {
  if (effectiveOG <= 1.0f) return 0.0f;
  float attenuation = ((effectiveOG - currentSG) / (effectiveOG - 1.0f)) * 100.0f;
  if (attenuation < 0.0f) return 0.0f;
  if (attenuation > 120.0f) return 120.0f;
  return attenuation;
}

float DerivedCalculations::abv(float effectiveOG, float currentSG) {
  if (effectiveOG <= 1.0f || currentSG <= 0.0f) return 0.0f;
  float result = (effectiveOG - currentSG) * 131.25f;
  return result < 0.0f ? 0.0f : result;
}

float DerivedCalculations::gravityDeltaPerHour(float previousSG, float currentSG, unsigned long deltaSeconds) {
  if (deltaSeconds == 0) return 0.0f;
  return (currentSG - previousSG) * 3600.0f / (float)deltaSeconds;
}

DerivedMetrics DerivedCalculations::compute(float effectiveOG, float currentSG, float previousSG,
                                            unsigned long deltaSeconds, int attenuationPercent) {
  DerivedMetrics metrics;
  metrics.expectedFinalGravity = expectedFG(effectiveOG, attenuationPercent);
  metrics.currentAttenuation = apparentAttenuation(effectiveOG, currentSG);
  metrics.estimatedABV = abv(effectiveOG, currentSG);
  metrics.gravityDeltaPerHour = gravityDeltaPerHour(previousSG, currentSG, deltaSeconds);
  metrics.fermentationRate = metrics.gravityDeltaPerHour < 0.0f ? -metrics.gravityDeltaPerHour : 0.0f;
  return metrics;
}
