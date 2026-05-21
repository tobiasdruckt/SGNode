#ifndef ETA_PREDICTOR_H
#define ETA_PREDICTOR_H

#include "brew_profile.h"
#include <stdint.h>

struct ETAResult {
  unsigned long secondsToFG;
  unsigned long secondsToPackaging;
  bool valid;
  bool presetBased;
  uint8_t confidencePercent;
};

class ETAPredictor {
public:
  static ETAResult predict(float currentSG, float expectedFG, float gravityDeltaPerHour);
  static ETAResult predict(const BrewProfile& profile, float currentSG, float expectedFG,
                           float gravityDeltaPerHour, unsigned long elapsedSeconds);
};

#endif
