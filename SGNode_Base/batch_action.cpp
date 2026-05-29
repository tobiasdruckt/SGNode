#include "batch_action.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

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

void BatchActionEngine::applyStyleDefaults(BrewProfile* profile) {
  if (!profile) return;
  profile->dryHopEnabled = styleIsHoppy(profile->beerStyle);
  if (profile->dryHopTriggerSG <= 0.0f) profile->dryHopTriggerSG = 1.014f;
  if (profile->dryHopContactHours == 0) profile->dryHopContactHours = 48;
}

BatchAction BatchActionEngine::evaluate(const BrewProfile& profile, FermentationPhase phase,
                                        float attenuation, float currentSG,
                                        float gravityDeltaPerHour, unsigned long nowEpoch) {
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

  if (profile.diacetylRestEnabled && !profile.dRestDone && !profile.dRestSkipped) {
    float trigger = yeastIsLagerLike(profile) ? 70.0f : 75.0f;
    if (attenuation >= trigger ||
        phase == FERMENTATION_DIACETYL_REST_READY) {
      return makeAction(ACTION_D_REST, 40, "Start D-rest", "Diacetyl rest is ready");
    }
  }

  if (profile.dryHopEnabled && !profile.dryHopDone && !profile.dryHopSkipped) {
    float triggerSG = profile.dryHopTriggerSG > 0.0f ? profile.dryHopTriggerSG : 1.014f;
    float fgBasedTrigger = profile.expectedFinalGravity > 1.0f ? profile.expectedFinalGravity + 0.004f : triggerSG;
    if (fgBasedTrigger > triggerSG) triggerSG = fgBasedTrigger;
    bool slopeFlat = fabs(gravityDeltaPerHour) < 0.00004f && attenuation >= 65.0f;
    if (currentSG <= triggerSG || slopeFlat) {
      char msg[80];
      snprintf(msg, sizeof(msg), "Add dry hops now, SG %.3f", currentSG);
      return makeAction(ACTION_DRY_HOP, 100, "Dry Hop", msg);
    }
    if (currentSG > triggerSG && gravityDeltaPerHour < -0.00001f) {
      float hours = (currentSG - triggerSG) / (-gravityDeltaPerHour);
      if (hours > 0.0f && hours < 240.0f) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Dry hop at SG %.3f", triggerSG);
        return makeFutureAction(ACTION_DRY_HOP, 100, "Dry Hop", msg, (unsigned long)(hours * 3600.0f));
      }
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

  bool dRestResolved = !profile.diacetylRestEnabled || profile.dRestDone || profile.dRestSkipped;
  bool dryHopResolved = !profile.dryHopEnabled || profile.dryHopSkipped ||
                        (profile.dryHopDone && (profile.dryHopRemoved || profile.dryHopRemoveSkipped));
  if (dRestResolved && dryHopResolved && !profile.coldCrashDone && !profile.coldCrashSkipped &&
      (phase == FERMENTATION_FINAL_GRAVITY_STABLE || phase == FERMENTATION_READY_TO_PACKAGE)) {
    return makeAction(ACTION_COLD_CRASH, 110, "Cold Crash", "Gravity is stable");
  }

  if (!profile.packageDone && !profile.packageSkipped &&
      (phase == FERMENTATION_READY_TO_PACKAGE || profile.coldCrashDone || profile.coldCrashSkipped)) {
    return makeAction(ACTION_PACKAGE, 120, "Package", "Ready for packaging");
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
    case ACTION_PACKAGE:
      profile->packageDone = true;
      profile->packagedAt = nowEpoch;
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
    case ACTION_PACKAGE:
      profile->packageSkipped = true;
      profile->packagedAt = nowEpoch;
      return true;
    default:
      return false;
  }
}

const char* BatchActionEngine::eventName(BatchActionType type, bool done) {
  switch (type) {
    case ACTION_D_REST: return done ? "D_REST_DONE" : "D_REST_SKIPPED";
    case ACTION_DRY_HOP: return done ? "DRY_HOP_DONE" : "DRY_HOP_SKIPPED";
    case ACTION_REMOVE_DRY_HOP: return done ? "DRY_HOP_REMOVED" : "DRY_HOP_REMOVE_SKIPPED";
    case ACTION_COLD_CRASH: return done ? "COLD_CRASH_DONE" : "COLD_CRASH_SKIPPED";
    case ACTION_PACKAGE: return done ? "PACKAGE_DONE" : "PACKAGE_SKIPPED";
    default: return done ? "ACTION_DONE" : "ACTION_SKIPPED";
  }
}
