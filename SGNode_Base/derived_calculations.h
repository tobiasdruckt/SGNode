#ifndef DERIVED_CALCULATIONS_H
#define DERIVED_CALCULATIONS_H

struct DerivedMetrics {
  float expectedFinalGravity;
  float currentAttenuation;
  float estimatedABV;
  float gravityDeltaPerHour;
  float fermentationRate;
};

class DerivedCalculations {
public:
  static float expectedFG(float og, int apparentAttenuationPercent);
  static float apparentAttenuation(float effectiveOG, float currentSG);
  static float abv(float effectiveOG, float currentSG);
  static float gravityDeltaPerHour(float previousSG, float currentSG, unsigned long deltaSeconds);
  static DerivedMetrics compute(float effectiveOG, float currentSG, float previousSG,
                                unsigned long deltaSeconds, int attenuationPercent);
};

#endif
