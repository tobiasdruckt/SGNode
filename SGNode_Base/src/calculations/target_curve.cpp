#include "target_curve.h"
#include <SD.h>
#include <string.h>

static float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

static float fermentationStartOffsetHours(const BrewProfile& profile) {
  if (profile.createdAt == 0 || profile.fermentationStartAt == 0 ||
      profile.fermentationStartAt <= profile.createdAt) {
    return 0.0f;
  }
  float offset = (profile.fermentationStartAt - profile.createdAt) / 3600.0f;
  if (offset < 0.0f) offset = 0.0f;
  if (offset > 168.0f) offset = 168.0f;
  return offset;
}

static float modelHourForChartHour(const BrewProfile& profile, float chartHour) {
  float modelHour = chartHour - fermentationStartOffsetHours(profile);
  return modelHour > 0.0f ? modelHour : 0.0f;
}

static void curveParams(const BrewProfile& profile, float* lagH, float* activeH, float* finishH, float* activeShare) {
  *lagH = profile.autoModeEnabled && profile.lagPhaseHours > 0.0f ? profile.lagPhaseHours : 8.0f;
  float totalH = profile.autoModeEnabled && profile.typicalDurationHours > 0.0f ? profile.typicalDurationHours : 144.0f;
  *activeShare = 0.78f;
  *activeH = totalH * 0.45f;
  *finishH = totalH * 0.45f;

  if (strcmp(profile.curveTemplate, "fast_ale") == 0) {
    *activeShare = 0.84f;
    *activeH = totalH * 0.36f;
    *finishH = totalH * 0.44f;
  } else if (strcmp(profile.curveTemplate, "very_fast_ale") == 0) {
    *activeShare = 0.88f;
    *activeH = totalH * 0.30f;
    *finishH = totalH * 0.38f;
  } else if (strcmp(profile.curveTemplate, "slow_clean_ale") == 0) {
    *activeShare = 0.72f;
    *activeH = totalH * 0.48f;
    *finishH = totalH * 0.46f;
  } else if (strcmp(profile.curveTemplate, "lager") == 0) {
    *activeShare = 0.70f;
    *activeH = totalH * 0.52f;
    *finishH = totalH * 0.42f;
  } else if (strcmp(profile.curveTemplate, "high_attenuation_saison") == 0) {
    *activeShare = 0.68f;
    *activeH = totalH * 0.42f;
    *finishH = totalH * 0.54f;
  }

  if (*activeH < 12.0f) *activeH = 12.0f;
  if (*finishH < 12.0f) *finishH = 12.0f;
}

float TargetCurveGenerator::expectedGravityAtHour(const BrewProfile& profile, float hour) {
  float fg = profile.expectedFinalGravity;
  float og = profile.effectiveOG > 1.0f ? profile.effectiveOG : profile.recipeOG;
  float progress = 0.0f;
  float lagH, activeH, finishH, activeShare;
  curveParams(profile, &lagH, &activeH, &finishH, &activeShare);
  float activeEnd = lagH + activeH;
  float finishEnd = activeEnd + finishH;

  if (hour < lagH) {
    progress = 0.02f * clamp01(hour / lagH);
  } else if (hour < activeEnd) {
    float t = clamp01((hour - lagH) / activeH);
    progress = 0.02f + activeShare * (1.0f - (1.0f - t) * (1.0f - t));
  } else if (hour < finishEnd) {
    float t = clamp01((hour - activeEnd) / finishH);
    progress = 0.02f + activeShare + (0.98f - activeShare) * t;
  } else {
    progress = 1.0f;
  }

  return og - (og - fg) * clamp01(progress);
}

float TargetCurveGenerator::expectedAttenuationAtHour(const BrewProfile& profile, float hour) {
  float og = profile.effectiveOG > 1.0f ? profile.effectiveOG : profile.recipeOG;
  float sg = expectedGravityAtHour(profile, hour);
  if (og <= 1.0f) return 0.0f;
  return ((og - sg) / (og - 1.0f)) * 100.0f;
}

bool TargetCurveGenerator::generateAndSave(const BrewProfile& profile) {
  if (!BrewProfileStore::ensureBatchDirectory(profile.batchId)) return false;
  char path[80];
  BrewProfileStore::targetPath(profile.batchId, path, sizeof(path));
  if (SD.exists(path)) SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;

  file.println("{");
  file.print("  \"batchId\":\""); file.print(profile.batchId); file.println("\",");
  file.print("  \"curveTemplate\":\""); file.print(profile.curveTemplate); file.println("\",");
  file.printf("  \"lagPhaseHours\":%.1f,\n", profile.lagPhaseHours);
  file.printf("  \"typicalDurationHours\":%.1f,\n", profile.typicalDurationHours);
  file.printf("  \"fermentationStartAt\":%lu,\n", profile.fermentationStartAt);
  file.printf("  \"startOffsetHours\":%.2f,\n", fermentationStartOffsetHours(profile));
  file.println("  \"model\":[\"lag\",\"active\",\"slow_finish\",\"stable_fg\"],");
  file.println("  \"points\":[");
  float startOffset = fermentationStartOffsetHours(profile);
  int maxHour = profile.autoModeEnabled && profile.typicalDurationHours > 0.0f
    ? (int)(profile.typicalDurationHours + 48.0f + startOffset)
    : 240;
  if (maxHour < 96) maxHour = 96;
  if (maxHour > 384) maxHour = 384;
  for (int hour = 0; hour <= maxHour; hour += 6) {
    float modelHour = modelHourForChartHour(profile, (float)hour);
    file.printf("    {\"hour\":%d,\"gravity\":%.5f,\"apparentAttenuation\":%.2f}%s\n",
                hour,
                expectedGravityAtHour(profile, modelHour),
                expectedAttenuationAtHour(profile, modelHour),
                hour + 6 <= maxHour ? "," : "");
  }
  file.println("  ]");
  file.println("}");
  file.close();
  return true;
}
