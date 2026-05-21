#ifndef TARGET_CURVE_H
#define TARGET_CURVE_H

#include "brew_profile.h"

class TargetCurveGenerator {
public:
  static bool generateAndSave(const BrewProfile& profile);
  static float expectedGravityAtHour(const BrewProfile& profile, float hour);
  static float expectedAttenuationAtHour(const BrewProfile& profile, float hour);
};

#endif
