#ifndef RECOMMENDATION_ENGINE_H
#define RECOMMENDATION_ENGINE_H

#include "fermentation_state_machine.h"
#include "brew_profile.h"

struct Recommendation {
  int code;
  char message[80];
};

class RecommendationEngine {
public:
  static Recommendation build(FermentationPhase phase, bool ogVerified, bool ogWarning,
                              float attenuation, float expectedFG, float currentSG);
  static Recommendation build(const BrewProfile& profile, FermentationPhase phase,
                              bool ogVerified, bool ogWarning, float attenuation,
                              float expectedFG, float currentSG, float temperatureC,
                              float gravityDeltaPerHour, unsigned long elapsedSeconds);
};

#endif
