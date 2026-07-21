#include "recommendation_engine.h"
#include <stdio.h>
#include <string.h>

Recommendation RecommendationEngine::sensorIssue(const char* detail) {
  Recommendation rec;
  rec.code = 90;
  if (detail && detail[0]) {
    snprintf(rec.message, sizeof(rec.message), "sensor issue: %s", detail);
  } else {
    strcpy(rec.message, "sensor issue: check float readings");
  }
  return rec;
}

Recommendation RecommendationEngine::build(FermentationPhase phase, bool ogVerified, bool ogWarning,
                                           float attenuation, float expectedFG, float currentSG) {
  Recommendation rec;
  rec.code = 0;
  strcpy(rec.message, "Start Brew Wizard to enable guidance");

  if (ogWarning) {
    rec.code = 10;
    strcpy(rec.message, "Verify OG before using fermentation guidance");
    return rec;
  }
  if (!ogVerified) {
    rec.code = 11;
    strcpy(rec.message, "Waiting for stable OG measurement");
    return rec;
  }

  switch (phase) {
    case FERMENTATION_PITCHED:
    case FERMENTATION_LAG_PHASE:
      rec.code = 20;
      strcpy(rec.message, "Fermentation pitched, waiting for activity");
      break;
    case FERMENTATION_ACTIVE:
      rec.code = 30;
      snprintf(rec.message, sizeof(rec.message), "Fermentation active, %.0f%% attenuated", attenuation);
      break;
    case FERMENTATION_DIACETYL_REST_READY:
      rec.code = 40;
      strcpy(rec.message, "Diacetyl rest should begin");
      break;
    case FERMENTATION_FINAL_GRAVITY_STABLE:
      rec.code = 50;
      strcpy(rec.message, "Gravity stable, confirm after 48 hours");
      break;
    case FERMENTATION_READY_TO_PACKAGE:
      rec.code = 60;
      strcpy(rec.message, "Gravity stable for 48 hours, ready to package");
      break;
    case FERMENTATION_COMPLETED:
      rec.code = 70;
      strcpy(rec.message, "Batch completed");
      break;
    default:
      rec.code = 21;
      snprintf(rec.message, sizeof(rec.message), "Target FG %.3f, current %.3f", expectedFG, currentSG);
      break;
  }

  return rec;
}

Recommendation RecommendationEngine::build(const BrewProfile& profile, FermentationPhase phase,
                                           bool ogVerified, bool ogWarning, float attenuation,
                                           float expectedFG, float currentSG, float temperatureC,
                                           float gravityDeltaPerHour, unsigned long elapsedSeconds) {
  Recommendation rec = build(phase, ogVerified, ogWarning, attenuation, expectedFG, currentSG);

  if ((profile.packageStarted || profile.packageDone) && !profile.packageSkipped && !profile.completed) {
    if (profile.temperatureProfile.enabled) {
      rec.code = 130;
      strcpy(rec.message, "Post-package temperature profile active");
    } else {
      rec.code = 130;
      strcpy(rec.message, "Packaged, finish via Manage Brew");
    }
    return rec;
  }

  if (!profile.autoModeEnabled) return rec;

  if (!ogVerified || ogWarning) return rec;

  if (temperatureC > 0.0f &&
      (temperatureC < profile.recommendedTempMinC || temperatureC > profile.recommendedTempMaxC)) {
    rec.code = 82;
    snprintf(rec.message, sizeof(rec.message), "Temp outside %.0f-%.0f C yeast range",
             profile.recommendedTempMinC, profile.recommendedTempMaxC);
    return rec;
  }

  if (phase == FERMENTATION_PITCHED || phase == FERMENTATION_LAG_PHASE) {
    rec.code = 80;
    snprintf(rec.message, sizeof(rec.message), "Auto Mode: using %s behavior", profile.selectedYeastPresetName);
    return rec;
  }

  float expectedProgress = 0.0f;
  if (profile.typicalDurationHours > 0.0f) {
    expectedProgress = ((float)elapsedSeconds / 3600.0f) / profile.typicalDurationHours * profile.expectedApparentAttenuation;
  }
  if (elapsedSeconds > 12UL * 3600UL && expectedProgress > 0.0f) {
    if (attenuation + 12.0f < expectedProgress) {
      rec.code = 83;
      strcpy(rec.message, "Fermentation slower than expected for yeast");
      return rec;
    }
    if (attenuation > expectedProgress + 18.0f && gravityDeltaPerHour < -0.00020f) {
      rec.code = 84;
      strcpy(rec.message, "Fermentation faster than expected for yeast");
      return rec;
    }
  }

  if (profile.diacetylRestRecommendedByYeast && !profile.dRestDone && !profile.dRestSkipped &&
      phase == FERMENTATION_ACTIVE && attenuation >= 65.0f) {
    rec.code = 85;
    strcpy(rec.message, "Selected yeast usually benefits from D-rest");
    return rec;
  }

  if (rec.code == 30) {
    rec.code = 81;
    snprintf(rec.message, sizeof(rec.message), "%s preset: %.0f%% attenuated", profile.selectedYeastPresetName, attenuation);
  }
  return rec;
}
