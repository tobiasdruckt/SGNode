#include "fermentation_state_machine.h"
#include <string.h>

FermentationStateMachine::FermentationStateMachine() {
  reset();
}

void FermentationStateMachine::reset() {
  currentPhase = FERMENTATION_IDLE;
  stableSinceEpoch = 0;
  firstDataEpoch = 0;
  activePeakSG = 0.0f;
  activeDropCount = 0;
}

FermentationPhase FermentationStateMachine::update(float currentSG, float expectedFG, float attenuation,
                                                   float gravityDeltaPerHour, float temperatureC,
                                                   bool diacetylRestEnabled, unsigned long nowEpoch) {
  (void)temperatureC;
  if (nowEpoch == 0) nowEpoch = firstDataEpoch + 600;
  if (firstDataEpoch == 0) firstDataEpoch = nowEpoch;

  float absSlope = gravityDeltaPerHour < 0.0f ? -gravityDeltaPerHour : gravityDeltaPerHour;
  bool nearFG = currentSG <= expectedFG + 0.004f;
  bool stable = absSlope <= 0.00008f;
  bool activelyDropping = gravityDeltaPerHour < -0.00015f;
  if (activePeakSG <= 1.0f || currentSG > activePeakSG) activePeakSG = currentSG;
  float dropFromPeak = activePeakSG - currentSG;

  if (currentPhase == FERMENTATION_IDLE) currentPhase = FERMENTATION_PITCHED;
  if (currentPhase == FERMENTATION_PITCHED && nowEpoch - firstDataEpoch >= 1800UL) currentPhase = FERMENTATION_LAG_PHASE;
  if (currentPhase == FERMENTATION_PITCHED || currentPhase == FERMENTATION_LAG_PHASE) {
    if (activelyDropping && dropFromPeak >= 0.0012f) activeDropCount++;
    else if (gravityDeltaPerHour > -0.00003f) activeDropCount = 0;
    if (activeDropCount >= 2 || (activelyDropping && dropFromPeak >= 0.0025f)) currentPhase = FERMENTATION_ACTIVE;
  }

  if (diacetylRestEnabled && attenuation >= 70.0f) {
    currentPhase = FERMENTATION_DIACETYL_REST_READY;
  }

  bool canBecomeStable = currentPhase == FERMENTATION_ACTIVE ||
                         currentPhase == FERMENTATION_DIACETYL_REST_READY ||
                         (attenuation >= 60.0f && nearFG);
  if (canBecomeStable && nearFG && stable) {
    if (stableSinceEpoch == 0) stableSinceEpoch = nowEpoch;
    if (nowEpoch - stableSinceEpoch >= 24UL * 3600UL) currentPhase = FERMENTATION_FINAL_GRAVITY_STABLE;
  } else if (!stable || !nearFG) {
    stableSinceEpoch = 0;
  }

  if (currentPhase == FERMENTATION_FINAL_GRAVITY_STABLE && stableSinceEpoch > 0 &&
      nowEpoch - stableSinceEpoch >= 48UL * 3600UL) {
    currentPhase = FERMENTATION_READY_TO_PACKAGE;
  }

  return currentPhase;
}

FermentationPhase FermentationStateMachine::update(const BrewProfile& profile, float currentSG, float expectedFG,
                                                   float attenuation, float gravityDeltaPerHour,
                                                   float temperatureC, unsigned long nowEpoch) {
  (void)temperatureC;
  if (!profile.autoModeEnabled) {
    return update(currentSG, expectedFG, attenuation, gravityDeltaPerHour,
                  temperatureC, profile.diacetylRestEnabled, nowEpoch);
  }

  if (nowEpoch == 0) nowEpoch = firstDataEpoch + 600;
  if (firstDataEpoch == 0) firstDataEpoch = nowEpoch;

  float absSlope = gravityDeltaPerHour < 0.0f ? -gravityDeltaPerHour : gravityDeltaPerHour;
  float activeThreshold = 0.00015f;
  if (strcmp(profile.fermentationSpeed, "very_fast") == 0) activeThreshold = 0.00024f;
  else if (strcmp(profile.fermentationSpeed, "fast") == 0) activeThreshold = 0.00020f;
  else if (strcmp(profile.fermentationSpeed, "slow") == 0) activeThreshold = 0.00008f;

  float dRestTrigger = 75.0f;
  if (strcmp(profile.curveTemplate, "lager") == 0 || strstr(profile.yeastCategory, "Lager") != NULL) {
    dRestTrigger = 62.0f;
  } else if (profile.diacetylRestRecommendedByYeast) {
    dRestTrigger = 72.0f;
  }

  bool nearFG = currentSG <= expectedFG + 0.004f;
  bool stable = absSlope <= 0.00008f;
  bool activelyDropping = gravityDeltaPerHour < -activeThreshold;
  if (activePeakSG <= 1.0f || currentSG > activePeakSG) activePeakSG = currentSG;
  float dropFromPeak = activePeakSG - currentSG;
  unsigned long lagSeconds = (unsigned long)(profile.lagPhaseHours * 3600.0f);
  if (lagSeconds < 1800UL) lagSeconds = 1800UL;

  if (currentPhase == FERMENTATION_IDLE) currentPhase = FERMENTATION_PITCHED;
  if (currentPhase == FERMENTATION_PITCHED && nowEpoch - firstDataEpoch >= lagSeconds) currentPhase = FERMENTATION_LAG_PHASE;
  if (currentPhase == FERMENTATION_PITCHED || currentPhase == FERMENTATION_LAG_PHASE) {
    if (activelyDropping && dropFromPeak >= 0.0012f) activeDropCount++;
    else if (gravityDeltaPerHour > -0.00003f) activeDropCount = 0;
    if (activeDropCount >= 2 || (activelyDropping && dropFromPeak >= 0.0025f)) currentPhase = FERMENTATION_ACTIVE;
  }

  bool shouldAutoRest = profile.diacetylRestRecommendedByYeast || profile.diacetylRestEnabled;
  if (shouldAutoRest && attenuation >= dRestTrigger) {
    currentPhase = FERMENTATION_DIACETYL_REST_READY;
  }

  bool canBecomeStable = currentPhase == FERMENTATION_ACTIVE ||
                         currentPhase == FERMENTATION_DIACETYL_REST_READY ||
                         (attenuation >= 60.0f && nearFG);
  if (canBecomeStable && nearFG && stable) {
    if (stableSinceEpoch == 0) stableSinceEpoch = nowEpoch;
    if (nowEpoch - stableSinceEpoch >= 24UL * 3600UL) currentPhase = FERMENTATION_FINAL_GRAVITY_STABLE;
  } else if (!stable || !nearFG) {
    stableSinceEpoch = 0;
  }

  if (currentPhase == FERMENTATION_FINAL_GRAVITY_STABLE && stableSinceEpoch > 0 &&
      nowEpoch - stableSinceEpoch >= 48UL * 3600UL) {
    currentPhase = FERMENTATION_READY_TO_PACKAGE;
  }

  return currentPhase;
}

FermentationPhase FermentationStateMachine::phase() const {
  return currentPhase;
}

const char* FermentationStateMachine::phaseName() const {
  return phaseName(currentPhase);
}

const char* FermentationStateMachine::phaseName(FermentationPhase phase) {
  switch (phase) {
    case FERMENTATION_IDLE: return "IDLE";
    case FERMENTATION_PITCHED: return "PITCHED";
    case FERMENTATION_LAG_PHASE: return "LAG PHASE";
    case FERMENTATION_ACTIVE: return "ACTIVE";
    case FERMENTATION_DIACETYL_REST_READY: return "D-REST READY";
    case FERMENTATION_FINAL_GRAVITY_STABLE: return "FG STABLE";
    case FERMENTATION_READY_TO_PACKAGE: return "PACKAGE";
    case FERMENTATION_COMPLETED: return "COMPLETED";
  }
  return "UNKNOWN";
}
