#ifndef BATCH_ACTION_H
#define BATCH_ACTION_H

#include "brew_profile.h"
#include "fermentation_state_machine.h"

enum BatchActionType {
  ACTION_NONE = 0,
  ACTION_VERIFY_OG,
  ACTION_D_REST,
  ACTION_DRY_HOP,
  ACTION_REMOVE_DRY_HOP,
  ACTION_COLD_CRASH,
  ACTION_PACKAGE,
  ACTION_FLOAT_ZERO_CAL,
  ACTION_FLOAT_IN_BREW
};

struct BatchAction {
  BatchActionType type;
  int code;
  bool requiresChoice;
  unsigned long secondsUntilDue;
  char title[32];
  char message[80];
};

class BatchActionEngine {
public:
  static void applyStyleDefaults(BrewProfile* profile);
  static BatchAction evaluate(const BrewProfile& profile, FermentationPhase phase,
                              float attenuation, float currentSG,
                              float gravityDeltaPerHour, unsigned long nowEpoch);
  static bool applyDone(BrewProfile* profile, BatchActionType type, unsigned long nowEpoch);
  static bool applySkip(BrewProfile* profile, BatchActionType type, unsigned long nowEpoch);
  static const char* eventName(BatchActionType type, bool done);
};

#endif
