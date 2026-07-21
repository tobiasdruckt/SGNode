#include "batch_action.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const unsigned long COLD_CRASH_DURATION_SECONDS = 48UL * 3600UL;

static unsigned long coldCrashDurationSeconds(const BrewProfile& profile) {
  if (profile.temperatureProfile.enabled && profile.temperatureProfile.advanced.crashHoldHours > 0) {
    return profile.temperatureProfile.advanced.crashHoldHours * 3600UL;
  }
  return COLD_CRASH_DURATION_SECONDS;
}

static bool styleIsHoppy(const char* style) {
  if (!style) return false;
  return strcmp(style, "IPA") == 0 ||
         strcmp(style, "Pale Ale") == 0 ||
         strcmp(style, "Hoppy Pils") == 0;
}

static bool yeastIsLagerLike(const BrewProfile& profile) {
  return strcmp(profile.curveTemplate, "lager") == 0 ||
         strstr(profile.yeastCategory, "Lager") != NULL;
}

static BatchAction makeAction(BatchActionType type, int code, const char* title, const char* message) {
  BatchAction action;
  memset(&action, 0, sizeof(action));
  action.type = type;
  action.code = code;
  action.requiresChoice = type != ACTION_NONE;
  strncpy(action.title, title, sizeof(action.title) - 1);
  strncpy(action.message, message, sizeof(action.message) - 1);
  return action;
}

static BatchAction makeFutureAction(BatchActionType type, int code, const char* title,
                                    const char* message, unsigned long secondsUntilDue) {
  BatchAction action = makeAction(type, code, title, message);
  action.requiresChoice = false;
  action.secondsUntilDue = secondsUntilDue;
  return action;
}

static void formatDaysHours(unsigned long seconds, char* buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0) return;
  unsigned long days = seconds / 86400UL;
  unsigned long hours = (seconds % 86400UL) / 3600UL;
  if (days > 0) snprintf(buffer, bufferSize, "%lud %luh", days, hours);
  else snprintf(buffer, bufferSize, "%luh", hours);
}

static BatchAction postPackageTemperatureAction(const BrewProfile& profile, unsigned long nowEpoch) {
  char message[80];
  unsigned long carbonationSeconds = profile.temperatureProfile.advanced.carbonationDays * 24UL * 3600UL;
  bool carbonationActive = profile.packagedAt == 0 || nowEpoch == 0 ||
                            nowEpoch < profile.packagedAt + carbonationSeconds;
  if (carbonationActive) {
    if (profile.packagedAt > 0 && nowEpoch > profile.packagedAt) {
      unsigned long elapsed = nowEpoch - profile.packagedAt;
      unsigned long remaining = carbonationSeconds > elapsed ? carbonationSeconds - elapsed : 0;
      char left[16];
      formatDaysHours(remaining, left, sizeof(left));
      snprintf(message, sizeof(message), "Hold %.0f C, %s left",
               profile.temperatureProfile.advanced.carbonationC, left);
    } else {
      snprintf(message, sizeof(message), "Hold %.0f C for carbonation",
               profile.temperatureProfile.advanced.carbonationC);
    }
    return makeFutureAction(ACTION_POST_PACKAGE_TEMP, 130, "Carbonation", message, 0);
  }

  snprintf(message, sizeof(message), "Storage %.0f C, end via Manage Brew",
           profile.temperatureProfile.advanced.storageC);
  return makeFutureAction(ACTION_POST_PACKAGE_TEMP, 131, "Storage", message, 0);
}

static float residualCO2Volumes(float temperatureC) {
  float tempF = temperatureC * 9.0f / 5.0f + 32.0f;
  return 3.0378f - 0.050062f * tempF + 0.00026555f * tempF * tempF;
}

static void formatPrimingSugarMessage(const BrewProfile& profile, char* message, size_t messageSize) {
  if (!message || messageSize == 0) return;
  float targetCO2 = profile.temperatureProfile.enabled ? profile.temperatureProfile.advanced.targetCO2 : 2.5f;
  float carbonationC = profile.temperatureProfile.enabled ? profile.temperatureProfile.advanced.carbonationC : 20.0f;
  float liters = profile.batchSizeLiters > 0.1f ? profile.batchSizeLiters : 20.0f;
  float residual = residualCO2Volumes(carbonationC);
  float co2ToAdd = targetCO2 - residual;
  if (co2ToAdd < 0.0f) co2ToAdd = 0.0f;
  float totalSugarG = co2ToAdd * liters * 3.82f;
  float sugarPerHalfLiterG = liters > 0.0f ? totalSugarG / (liters * 2.0f) : 0.0f;
  snprintf(message, messageSize, "Sugar %.1fg/0.5L, %.0fg total",
           sugarPerHalfLiterG, totalSugarG);
}

void BatchActionEngine::applyStyleDefaults(BrewProfile* profile) {
  if (!profile) return;
  profile->dryHopEnabled = styleIsHoppy(profile->beerStyle);
  profile->dryHopFgOffset = profile->dryHopEnabled ? 0.004f : 0.0f;
  if (profile->dryHopContactHours == 0) profile->dryHopContactHours = profile->dryHopEnabled ? 48 : 0;
  if (profile->expectedFinalGravity > 1.0f) {
    profile->dryHopTriggerSG = profile->expectedFinalGravity + profile->dryHopFgOffset;
  } else if (profile->dryHopTriggerSG <= 0.0f) {
    profile->dryHopTriggerSG = 1.014f;
  }
}

BatchAction BatchActionEngine::evaluate(const BrewProfile& profile, FermentationPhase phase,
                                        float attenuation, float currentSG,
                                        float gravityDeltaPerHour, unsigned long nowEpoch) {
  bool postPackageTracking = (profile.packageStarted || profile.packageDone) &&
                             !profile.packageSkipped && !profile.completed;
  if (postPackageTracking) {
    if (profile.temperatureProfile.enabled) {
      return postPackageTemperatureAction(profile, nowEpoch);
    }
    BatchAction none = makeAction(ACTION_NONE, 0, "Next", "No action required");
    none.requiresChoice = false;
    return none;
  }

  if (!profile.floatZeroCalDone && !profile.floatZeroCalSkipped) {
    return makeAction(ACTION_FLOAT_ZERO_CAL, 12, "Level Float", "Zero angle on a flat surface");
  }

  if (!profile.completed && !profile.ogVerified && !profile.floatInBrewConfirmed) {
    return makeAction(ACTION_FLOAT_IN_BREW, 13, "Put Float In Brew", "Confirm after placing float in wort");
  }

  if (profile.ogNeedsChoice) {
    BatchAction action = makeAction(ACTION_VERIFY_OG, 10, "Verify OG", "Choose recipe or measured OG");
    action.requiresChoice = false;
    return action;
  }

  if (!profile.ogVerified) {
    BatchAction none = makeAction(ACTION_NONE, 11, "Measuring OG", "Waiting for stable OG");
    none.requiresChoice = false;
    return none;
  }

  bool dRestConfigured = profile.temperatureProfile.enabled
    ? profile.temperatureProfile.advanced.dRestHoldHours > 0
    : profile.diacetylRestEnabled;
  bool coldCrashConfigured = !profile.temperatureProfile.enabled ||
                             profile.temperatureProfile.advanced.crashHoldHours > 0;

  if (profile.diacetylRestEnabled && dRestConfigured && !profile.dRestDone && !profile.dRestSkipped) {
    float trigger = yeastIsLagerLike(profile) ? 70.0f : 75.0f;
    if (attenuation >= trigger ||
        phase == FERMENTATION_DIACETYL_REST_READY) {
      return makeAction(ACTION_D_REST, 40, "Start D-rest", "Diacetyl rest is ready");
    }
  }

  if (profile.dryHopEnabled && !profile.dryHopDone && !profile.dryHopSkipped) {
    float triggerSG = profile.dryHopTriggerSG > 0.0f ? profile.dryHopTriggerSG : 1.014f;
    float fgBasedTrigger = profile.expectedFinalGravity > 1.0f
      ? profile.expectedFinalGravity + (profile.dryHopFgOffset > 0.0f ? profile.dryHopFgOffset : 0.004f)
      : triggerSG;
    if (fgBasedTrigger > triggerSG) triggerSG = fgBasedTrigger;
    bool slopeFlat = fabs(gravityDeltaPerHour) < 0.00004f && attenuation >= 65.0f;
    if (currentSG <= triggerSG || slopeFlat) {
      char msg[80];
      snprintf(msg, sizeof(msg), "Add dry hops now, SG %.3f", currentSG);
      return makeAction(ACTION_DRY_HOP, 100, "Dry Hop", msg);
    }
    if (currentSG > triggerSG && gravityDeltaPerHour < -0.00001f) {
      float hours = (currentSG - triggerSG) / (-gravityDeltaPerHour);
      if (hours > 0.0f && hours <= 12.0f && currentSG - triggerSG <= 0.012f) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Dry hop at SG %.3f", triggerSG);
        return makeFutureAction(ACTION_DRY_HOP, 100, "Dry Hop", msg, (unsigned long)(hours * 3600.0f));
      }
    }
    if (currentSG > triggerSG) {
      char msg[80];
      snprintf(msg, sizeof(msg), "Dry hop later at SG %.3f", triggerSG);
      return makeFutureAction(ACTION_DRY_HOP, 100, "Dry Hop", msg, 0);
    }
  }

  if (profile.dryHopDone && !profile.dryHopRemoved && !profile.dryHopRemoveSkipped) {
    unsigned long contactSeconds = profile.dryHopContactHours > 0 ? profile.dryHopContactHours * 3600UL : 48UL * 3600UL;
    if (nowEpoch > 0 && profile.dryHopStartTime > 0 && nowEpoch >= profile.dryHopStartTime + contactSeconds) {
      return makeAction(ACTION_REMOVE_DRY_HOP, 101, "Remove Dry Hop", "Contact time reached");
    }
    if (nowEpoch > 0 && profile.dryHopStartTime > 0 && nowEpoch < profile.dryHopStartTime + contactSeconds) {
      return makeFutureAction(ACTION_REMOVE_DRY_HOP, 101, "Remove Dry Hop",
                              "Waiting for contact time",
                              profile.dryHopStartTime + contactSeconds - nowEpoch);
    }
  }

  bool dRestResolved = !profile.diacetylRestEnabled || !dRestConfigured || profile.dRestDone || profile.dRestSkipped;
  bool dryHopResolved = !profile.dryHopEnabled || profile.dryHopSkipped ||
                        (profile.dryHopDone && (profile.dryHopRemoved || profile.dryHopRemoveSkipped));
  if (dRestResolved && dryHopResolved && coldCrashConfigured &&
      !profile.coldCrashDone && !profile.coldCrashSkipped &&
      (phase == FERMENTATION_FINAL_GRAVITY_STABLE || phase == FERMENTATION_READY_TO_PACKAGE)) {
    return makeAction(ACTION_COLD_CRASH, 110, "Cold Crash", "Gravity is stable");
  }

  if (!profile.packageStarted && !profile.packageDone && !profile.packageSkipped) {
    unsigned long coldCrashSeconds = coldCrashDurationSeconds(profile);
    if (profile.coldCrashDone && profile.coldCrashStartedAt > 0 &&
        nowEpoch > 0 && nowEpoch < profile.coldCrashStartedAt + coldCrashSeconds) {
      return makeFutureAction(ACTION_PACKAGE_START, 120, "Start Packaging", "Cold crash in progress",
                              profile.coldCrashStartedAt + coldCrashSeconds - nowEpoch);
    }
    bool coldCrashElapsed = profile.coldCrashDone &&
                            profile.coldCrashStartedAt > 0 &&
                            nowEpoch >= profile.coldCrashStartedAt + coldCrashSeconds;
    if (!coldCrashConfigured || profile.coldCrashSkipped || coldCrashElapsed) {
      char message[80];
      formatPrimingSugarMessage(profile, message, sizeof(message));
      return makeAction(ACTION_PACKAGE_START, 120, "Start Packaging", message);
    }
  }

  if (profile.packageStarted && !profile.packageDone && !profile.packageSkipped) {
    return makeAction(ACTION_PACKAGE_COMPLETE, 121, "Packaging Complete",
                      "Confirm when bottles or keg are sealed");
  }

  if (profile.packageDone && !profile.completed && profile.temperatureProfile.enabled) {
    return postPackageTemperatureAction(profile, nowEpoch);
  }

  BatchAction none = makeAction(ACTION_NONE, 0, "Next", "No action required");
  none.requiresChoice = false;
  return none;
}

bool BatchActionEngine::applyDone(BrewProfile* profile, BatchActionType type, unsigned long nowEpoch) {
  if (!profile) return false;
  switch (type) {
    case ACTION_D_REST:
      profile->dRestDone = true;
      profile->dRestStartedAt = nowEpoch;
      return true;
    case ACTION_DRY_HOP:
      profile->dryHopDone = true;
      profile->dryHopStartTime = nowEpoch;
      return true;
    case ACTION_REMOVE_DRY_HOP:
      profile->dryHopRemoved = true;
      profile->dryHopRemovedAt = nowEpoch;
      return true;
    case ACTION_COLD_CRASH:
      profile->coldCrashDone = true;
      profile->coldCrashStartedAt = nowEpoch;
      return true;
    case ACTION_PACKAGE_START:
      profile->packageStarted = true;
      profile->packageStartedAt = nowEpoch;
      return true;
    case ACTION_PACKAGE_COMPLETE:
      profile->packageStarted = true;
      if (profile->packageStartedAt == 0) profile->packageStartedAt = nowEpoch;
      profile->packageDone = true;
      profile->packagedAt = nowEpoch;
      return true;
    case ACTION_FLOAT_ZERO_CAL:
      profile->floatZeroCalDone = true;
      profile->floatZeroCalibratedAt = nowEpoch;
      return true;
    case ACTION_FLOAT_IN_BREW:
      profile->floatInBrewConfirmed = true;
      profile->floatInBrewAt = nowEpoch;
      if (nowEpoch > 0) profile->createdAt = nowEpoch;
      profile->fermentationStartAt = 0;
      profile->ogVerified = false;
      profile->ogNeedsChoice = false;
      return true;
    default:
      return false;
  }
}

bool BatchActionEngine::applySkip(BrewProfile* profile, BatchActionType type, unsigned long nowEpoch) {
  if (!profile) return false;
  switch (type) {
    case ACTION_D_REST:
      profile->dRestSkipped = true;
      profile->dRestStartedAt = nowEpoch;
      return true;
    case ACTION_DRY_HOP:
      profile->dryHopSkipped = true;
      profile->dryHopStartTime = nowEpoch;
      return true;
    case ACTION_REMOVE_DRY_HOP:
      profile->dryHopRemoveSkipped = true;
      profile->dryHopRemovedAt = nowEpoch;
      return true;
    case ACTION_COLD_CRASH:
      profile->coldCrashSkipped = true;
      profile->coldCrashStartedAt = nowEpoch;
      return true;
    case ACTION_PACKAGE_START:
      profile->packageSkipped = true;
      profile->packageStartedAt = nowEpoch;
      return true;
    case ACTION_PACKAGE_COMPLETE:
      profile->packageSkipped = true;
      profile->packagedAt = nowEpoch;
      return true;
    case ACTION_FLOAT_ZERO_CAL:
      profile->floatZeroCalSkipped = true;
      profile->floatZeroCalibratedAt = nowEpoch;
      return true;
    case ACTION_FLOAT_IN_BREW:
      return false;
    default:
      return false;
  }
}

const char* BatchActionEngine::eventName(BatchActionType type, bool done) {
  switch (type) {
    case ACTION_D_REST: return done ? "D_REST_DONE" : "D_REST_SKIPPED";
    case ACTION_DRY_HOP: return done ? "DRY_HOP_DONE" : "DRY_HOP_SKIPPED";
    case ACTION_REMOVE_DRY_HOP: return done ? "DRY_HOP_REMOVED" : "DRY_HOP_REMOVE_SKIPPED";
    case ACTION_COLD_CRASH: return done ? "COLD_CRASH_STARTED" : "COLD_CRASH_SKIPPED";
    case ACTION_PACKAGE_START: return done ? "PACKAGE_STARTED" : "PACKAGE_SKIPPED";
    case ACTION_PACKAGE_COMPLETE: return done ? "PACKAGE_DONE" : "PACKAGE_SKIPPED";
    case ACTION_FLOAT_ZERO_CAL: return done ? "FLOAT_ZERO_CAL_REQUESTED" : "FLOAT_ZERO_CAL_SKIPPED";
    case ACTION_FLOAT_IN_BREW: return done ? "FLOAT_IN_BREW_CONFIRMED" : "FLOAT_IN_BREW_SKIPPED";
    default: return done ? "ACTION_DONE" : "ACTION_SKIPPED";
  }
}
