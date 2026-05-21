#ifndef FERMENTATION_STATE_MACHINE_H
#define FERMENTATION_STATE_MACHINE_H

#include "brew_profile.h"

enum FermentationPhase {
  FERMENTATION_IDLE,
  FERMENTATION_PITCHED,
  FERMENTATION_LAG_PHASE,
  FERMENTATION_ACTIVE,
  FERMENTATION_DIACETYL_REST_READY,
  FERMENTATION_FINAL_GRAVITY_STABLE,
  FERMENTATION_READY_TO_PACKAGE,
  FERMENTATION_COMPLETED
};

class FermentationStateMachine {
public:
  FermentationStateMachine();
  void reset();
  FermentationPhase update(float currentSG, float expectedFG, float attenuation,
                           float gravityDeltaPerHour, float temperatureC,
                           bool diacetylRestEnabled, unsigned long nowEpoch);
  FermentationPhase update(const BrewProfile& profile, float currentSG, float expectedFG,
                           float attenuation, float gravityDeltaPerHour,
                           float temperatureC, unsigned long nowEpoch);
  FermentationPhase phase() const;
  const char* phaseName() const;
  static const char* phaseName(FermentationPhase phase);

private:
  FermentationPhase currentPhase;
  unsigned long stableSinceEpoch;
  unsigned long firstDataEpoch;
};

#endif
