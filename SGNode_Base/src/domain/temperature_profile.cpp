#include "temperature_profile.h"
#include "brew_profile.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {

float clampFloat(float value, float minimum, float maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

float finiteOrDefault(float value, float fallback) {
  return (!isnan(value) && !isinf(value)) ? value : fallback;
}

bool textEquals(const char* left, const char* right) {
  return left && right && strcmp(left, right) == 0;
}

bool textContains(const char* text, const char* needle) {
  return text && needle && strstr(text, needle) != NULL;
}

bool styleIs(const BrewProfile& profile, const char* style) {
  return strcmp(profile.beerStyle, style) == 0;
}

float styleMainShiftC(const BrewProfile& profile) {
  if (styleIs(profile, "Wheat Beer")) return 1.0f;
  if (styleIs(profile, "Stout") || styleIs(profile, "Porter")) return -0.5f;
  return 0.0f;
}

float rangeLow(const BrewProfile& profile) {
  float low = finiteOrDefault(profile.recommendedTempMinC, 18.0f);
  if (low < -2.0f || low > 45.0f) low = 18.0f;
  return low;
}

float rangeHigh(const BrewProfile& profile, float low) {
  float high = finiteOrDefault(profile.recommendedTempMaxC, low + 4.0f);
  if (high <= low) high = low + 4.0f;
  if (high > 45.0f) high = 45.0f;
  return high;
}

void safeCopy(char* target, size_t targetSize, const char* source) {
  if (!target || targetSize == 0) return;
  if (!source) source = "";
  strncpy(target, source, targetSize - 1);
  target[targetSize - 1] = '\0';
}

void addPhase(TemperatureProfile* profile, const char* id, const char* label, float targetC,
              unsigned long holdHours, const char* triggerType, const char* rampClass,
              bool requiresActionResolved, const char* autoStartPolicy) {
  if (!profile || profile->phaseCount >= TEMPERATURE_PROFILE_MAX_PHASES) return;
  TemperatureProfilePhase& phase = profile->phases[profile->phaseCount++];
  memset(&phase, 0, sizeof(phase));
  safeCopy(phase.id, sizeof(phase.id), id);
  safeCopy(phase.label, sizeof(phase.label), label);
  phase.targetC = targetC;
  phase.holdHoursAfterReached = holdHours;
  safeCopy(phase.triggerType, sizeof(phase.triggerType), triggerType);
  safeCopy(phase.rampClass, sizeof(phase.rampClass), rampClass);
  phase.requiresActionResolved = requiresActionResolved;
  safeCopy(phase.autoStartPolicy, sizeof(phase.autoStartPolicy), autoStartPolicy);
}

void rebuildPhases(TemperatureProfile* profile) {
  if (!profile) return;
  profile->phaseCount = 0;
  addPhase(profile, "pitch", "Pitch", profile->advanced.pitchC, 0, "batch_start", "normal", false, "auto");
  addPhase(profile, "main", "Main", profile->advanced.mainC, profile->advanced.mainHoldHours,
           "fermentation", "normal", false, "auto");
  if (profile->advanced.dRestHoldHours > 0) {
    addPhase(profile, "d_rest", "D-rest", profile->advanced.dRestC, profile->advanced.dRestHoldHours,
             "attenuation", "normal", true, "manual_or_auto");
  }
  if (profile->advanced.crashHoldHours > 0) {
    addPhase(profile, "cold_crash", "Cold Crash", profile->advanced.crashC, profile->advanced.crashHoldHours,
             "fg_stable", "cold_crash", true, "manual_or_auto");
  }
  addPhase(profile, "carbonation", "Carbonation", profile->advanced.carbonationC,
           profile->advanced.carbonationDays * 24UL, "package_done", "normal", true, "manual");
  addPhase(profile, "storage", "Storage", profile->advanced.storageC,
           profile->advanced.storageDaysHint * 24UL, "carbonation_done", "normal", false, "auto");
}

void jsonEscapePrint(fs::File& file, const char* text) {
  for (const char* p = text; p && *p; ++p) {
    if (*p == '"' || *p == '\\') file.print('\\');
    if ((unsigned char)*p >= 32) file.print(*p);
  }
}

float extractFloat(const char* json, const char* key, float fallback) {
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char* start = strstr(json, pattern);
  if (!start) return fallback;
  start += strlen(pattern);
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
  if (*start != ':') return fallback;
  start++;
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
  return atof(start);
}

unsigned long extractULong(const char* json, const char* key, unsigned long fallback) {
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char* start = strstr(json, pattern);
  if (!start) return fallback;
  start += strlen(pattern);
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
  if (*start != ':') return fallback;
  start++;
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
  return strtoul(start, NULL, 10);
}

bool extractBool(const char* json, const char* key, bool fallback) {
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char* start = strstr(json, pattern);
  if (!start) return fallback;
  start += strlen(pattern);
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
  if (*start != ':') return fallback;
  start++;
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
  return strncmp(start, "true", 4) == 0;
}

bool extractString(const char* json, const char* key, char* out, size_t outSize) {
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char* start = strstr(json, pattern);
  if (!start) return false;
  start += strlen(pattern);
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
  if (*start != ':') return false;
  start++;
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
  if (*start != '"') return false;
  start++;
  const char* end = strchr(start, '"');
  if (!end) return false;
  size_t len = end - start;
  if (len >= outSize) len = outSize - 1;
  memcpy(out, start, len);
  out[len] = '\0';
  return true;
}

void writeStringField(fs::File& file, const char* indent, const char* key, const char* value, bool comma) {
  file.print(indent);
  file.print("\"");
  file.print(key);
  file.print("\":\"");
  jsonEscapePrint(file, value);
  file.print("\"");
  file.println(comma ? "," : "");
}

void writeFloatField(fs::File& file, const char* indent, const char* key, float value, bool comma) {
  file.print(indent);
  file.print("\"");
  file.print(key);
  file.print("\":");
  file.printf("%.2f", value);
  file.println(comma ? "," : "");
}

void writeULongField(fs::File& file, const char* indent, const char* key, unsigned long value, bool comma) {
  file.print(indent);
  file.print("\"");
  file.print(key);
  file.print("\":");
  file.print(value);
  file.println(comma ? "," : "");
}

}  // namespace

void TemperatureProfileEngine::setDefaults(TemperatureProfile* profile) {
  if (!profile) return;
  memset(profile, 0, sizeof(TemperatureProfile));
  profile->enabled = false;
  profile->version = 1;
  safeCopy(profile->family, sizeof(profile->family), "clean_ale");
  safeCopy(profile->presetId, sizeof(profile->presetId), "");
  safeCopy(profile->source, sizeof(profile->source), "generated");
  profile->advanced.pitchC = 18.0f;
  profile->advanced.mainC = 19.0f;
  profile->advanced.dRestC = 21.0f;
  profile->advanced.crashC = 4.0f;
  profile->advanced.lagerC = 4.0f;
  profile->advanced.mainHoldHours = 72;
  profile->advanced.dRestHoldHours = 0;
  profile->advanced.crashHoldHours = 48;
  profile->advanced.targetCO2 = 2.5f;
  profile->advanced.carbonationC = 20.0f;
  profile->advanced.carbonationDays = 14;
  profile->advanced.storageC = 4.0f;
  profile->advanced.storageDaysHint = 14;
  profile->advanced.normalRampKPerH = 0.2f;
  profile->advanced.coldCrashRampKPerH = 1.0f;
  safeCopy(profile->status.activePhaseId, sizeof(profile->status.activePhaseId), "pitch");
  rebuildPhases(profile);
}

void TemperatureProfileEngine::generateForProfile(BrewProfile* profile) {
  if (!profile) return;
  TemperatureProfile* temp = &profile->temperatureProfile;
  setDefaults(temp);
  temp->enabled = true;
  temp->version = 1;
  safeCopy(temp->presetId, sizeof(temp->presetId), profile->selectedYeastPresetId);
  safeCopy(temp->source, sizeof(temp->source), "generated");

  const char* family = profile->curveTemplate[0] ? profile->curveTemplate : "clean_ale";
  safeCopy(temp->family, sizeof(temp->family), family);

  float low = rangeLow(*profile);
  float high = rangeHigh(*profile, low);
  float span = high - low;
  float mainC = low + span * 0.35f;
  float pitchC = low;
  float dRestC = high;
  unsigned long mainHoldHours = 72;
  unsigned long dRestHoldHours = profile->diacetylRestEnabled || profile->diacetylRestRecommendedByYeast ? 48 : 0;
  float crashC = 4.0f;
  float lagerC = 4.0f;
  float carbC = 20.0f;
  unsigned long carbDays = 14;
  unsigned long storageDays = 14;

  if (textEquals(profile->selectedYeastPresetId, "w3470")) {
    pitchC = 9.0f;
    mainC = 10.0f;
    dRestC = 15.0f;
    crashC = 4.0f;
    lagerC = 2.0f;
    mainHoldHours = 168;
    dRestHoldHours = 48;
    storageDays = 28;
  } else if (textEquals(family, "lager") || textContains(profile->yeastCategory, "Lager")) {
    pitchC = low;
    mainC = clampFloat(low + 1.0f, low, high);
    dRestC = clampFloat(high, low, 18.0f);
    crashC = 4.0f;
    lagerC = 2.0f;
    mainHoldHours = 168;
    dRestHoldHours = profile->diacetylRestEnabled || profile->diacetylRestRecommendedByYeast ? 48 : 0;
    storageDays = 28;
  } else if (textEquals(family, "fast_ale")) {
    pitchC = clampFloat(low, low, high);
    mainC = clampFloat(low + span * 0.45f, low, high);
    dRestC = clampFloat(high, low, high);
    mainHoldHours = 48;
    carbC = clampFloat(mainC + 1.0f, 18.0f, 22.0f);
  } else if (textEquals(family, "slow_clean_ale")) {
    pitchC = low;
    mainC = clampFloat(low + span * 0.25f, low, high);
    dRestC = clampFloat(high, low, high);
    mainHoldHours = 96;
    dRestHoldHours = profile->diacetylRestEnabled || profile->diacetylRestRecommendedByYeast ? 48 : 0;
  } else if (textEquals(family, "very_fast_ale")) {
    pitchC = clampFloat(low + span * 0.25f, low, high);
    mainC = clampFloat(low + span * 0.45f, low, high);
    dRestC = mainC;
    mainHoldHours = 24;
    dRestHoldHours = 0;
    carbC = 22.0f;
    carbDays = 7;
  } else if (textEquals(family, "high_attenuation_saison")) {
    pitchC = clampFloat(low + 1.0f, low, high);
    mainC = clampFloat(low + span * 0.70f, low, high);
    dRestC = mainC;
    mainHoldHours = 96;
    dRestHoldHours = 0;
    carbC = 22.0f;
  }

  mainC = clampFloat(mainC + styleMainShiftC(*profile), low, high);
  if (!textEquals(profile->selectedYeastPresetId, "w3470") && !textEquals(family, "lager")) {
    pitchC = clampFloat(pitchC, low, high);
    dRestC = clampFloat(dRestC, low, high);
  }

  temp->advanced.pitchC = pitchC;
  temp->advanced.mainC = mainC;
  temp->advanced.dRestC = dRestC;
  temp->advanced.crashC = crashC;
  temp->advanced.lagerC = lagerC;
  temp->advanced.mainHoldHours = mainHoldHours;
  temp->advanced.dRestHoldHours = dRestHoldHours;
  temp->advanced.crashHoldHours = 48;
  temp->advanced.targetCO2 = styleIs(*profile, "Wheat Beer") ? 3.0f : (styleIs(*profile, "Stout") ? 2.1f : 2.5f);
  temp->advanced.carbonationC = carbC;
  temp->advanced.carbonationDays = carbDays;
  temp->advanced.storageC = lagerC;
  temp->advanced.storageDaysHint = storageDays;
  temp->advanced.normalRampKPerH = 0.2f;
  temp->advanced.coldCrashRampKPerH = 1.0f;
  safeCopy(temp->status.activePhaseId, sizeof(temp->status.activePhaseId), "pitch");
  rebuildPhases(temp);
}

bool TemperatureProfileEngine::loadFromJson(const char* json, TemperatureProfile* profile) {
  if (!json || !profile) return false;
  const char* section = strstr(json, "\"temperatureProfile\"");
  if (!section) return false;

  setDefaults(profile);
  profile->enabled = true;
  profile->version = (uint8_t)extractULong(section, "version", 1);
  extractString(section, "family", profile->family, sizeof(profile->family));
  extractString(section, "presetId", profile->presetId, sizeof(profile->presetId));
  extractString(section, "source", profile->source, sizeof(profile->source));
  profile->advanced.pitchC = extractFloat(section, "pitchC", profile->advanced.pitchC);
  profile->advanced.mainC = extractFloat(section, "mainC", profile->advanced.mainC);
  profile->advanced.dRestC = extractFloat(section, "dRestC", profile->advanced.dRestC);
  profile->advanced.crashC = extractFloat(section, "crashC", profile->advanced.crashC);
  profile->advanced.lagerC = extractFloat(section, "lagerC", profile->advanced.lagerC);
  profile->advanced.mainHoldHours = extractULong(section, "mainHoldHours", profile->advanced.mainHoldHours);
  profile->advanced.dRestHoldHours = extractULong(section, "dRestHoldHours", profile->advanced.dRestHoldHours);
  profile->advanced.crashHoldHours = extractULong(section, "crashHoldHours", profile->advanced.crashHoldHours);
  profile->advanced.targetCO2 = extractFloat(section, "targetCO2", profile->advanced.targetCO2);
  profile->advanced.carbonationC = extractFloat(section, "carbonationC", profile->advanced.carbonationC);
  profile->advanced.carbonationDays = extractULong(section, "carbonationDays", profile->advanced.carbonationDays);
  profile->advanced.storageC = extractFloat(section, "storageC", profile->advanced.storageC);
  profile->advanced.storageDaysHint = extractULong(section, "storageDaysHint", profile->advanced.storageDaysHint);
  profile->advanced.normalRampKPerH = extractFloat(section, "normalRampKPerH", profile->advanced.normalRampKPerH);
  profile->advanced.coldCrashRampKPerH = extractFloat(section, "coldCrashRampKPerH", profile->advanced.coldCrashRampKPerH);
  extractString(section, "activePhaseId", profile->status.activePhaseId, sizeof(profile->status.activePhaseId));
  profile->status.targetReached = extractBool(section, "targetReached", profile->status.targetReached);
  profile->status.targetReachedAt = extractULong(section, "targetReachedAt", profile->status.targetReachedAt);
  profile->status.phaseStartedAt = extractULong(section, "phaseStartedAt", profile->status.phaseStartedAt);
  profile->status.autoStarted = extractBool(section, "autoStarted", profile->status.autoStarted);
  rebuildPhases(profile);
  return true;
}

void TemperatureProfileEngine::rebuildPhaseList(TemperatureProfile* profile) {
  rebuildPhases(profile);
}

void TemperatureProfileEngine::writeJson(fs::File& file, const TemperatureProfile& profile,
                                         const char* indent, bool trailingComma) {
  const char* i = indent ? indent : "";
  file.print(i); file.println("\"temperatureProfile\":{");
  file.print(i); file.printf("  \"version\":%u,\n", profile.version);
  writeStringField(file, "    ", "family", profile.family, true);
  writeStringField(file, "    ", "presetId", profile.presetId, true);
  writeStringField(file, "    ", "source", profile.source, true);
  file.print(i); file.println("  \"phases\":[");
  for (uint8_t idx = 0; idx < profile.phaseCount; idx++) {
    const TemperatureProfilePhase& phase = profile.phases[idx];
    file.print(i); file.println("    {");
    writeStringField(file, "      ", "id", phase.id, true);
    writeStringField(file, "      ", "label", phase.label, true);
    writeFloatField(file, "      ", "targetC", phase.targetC, true);
    writeULongField(file, "      ", "holdHoursAfterReached", phase.holdHoursAfterReached, true);
    writeStringField(file, "      ", "triggerType", phase.triggerType, true);
    writeStringField(file, "      ", "rampClass", phase.rampClass, true);
    file.print("      \"requiresActionResolved\":");
    file.print(phase.requiresActionResolved ? "true" : "false");
    file.println(",");
    writeStringField(file, "      ", "autoStartPolicy", phase.autoStartPolicy, false);
    file.print(i); file.print("    }");
    file.println(idx + 1 < profile.phaseCount ? "," : "");
  }
  file.print(i); file.println("  ],");
  file.print(i); file.println("  \"advanced\":{");
  writeFloatField(file, "    ", "pitchC", profile.advanced.pitchC, true);
  writeFloatField(file, "    ", "mainC", profile.advanced.mainC, true);
  writeFloatField(file, "    ", "dRestC", profile.advanced.dRestC, true);
  writeFloatField(file, "    ", "crashC", profile.advanced.crashC, true);
  writeFloatField(file, "    ", "lagerC", profile.advanced.lagerC, true);
  writeULongField(file, "    ", "mainHoldHours", profile.advanced.mainHoldHours, true);
  writeULongField(file, "    ", "dRestHoldHours", profile.advanced.dRestHoldHours, true);
  writeULongField(file, "    ", "crashHoldHours", profile.advanced.crashHoldHours, true);
  writeFloatField(file, "    ", "targetCO2", profile.advanced.targetCO2, true);
  writeFloatField(file, "    ", "carbonationC", profile.advanced.carbonationC, true);
  writeULongField(file, "    ", "carbonationDays", profile.advanced.carbonationDays, true);
  writeFloatField(file, "    ", "storageC", profile.advanced.storageC, true);
  writeULongField(file, "    ", "storageDaysHint", profile.advanced.storageDaysHint, true);
  writeFloatField(file, "    ", "normalRampKPerH", profile.advanced.normalRampKPerH, true);
  writeFloatField(file, "    ", "coldCrashRampKPerH", profile.advanced.coldCrashRampKPerH, false);
  file.print(i); file.println("  },");
  file.print(i); file.println("  \"status\":{");
  writeStringField(file, "    ", "activePhaseId", profile.status.activePhaseId, true);
  file.print("    \"targetReached\":"); file.print(profile.status.targetReached ? "true" : "false"); file.println(",");
  writeULongField(file, "    ", "targetReachedAt", profile.status.targetReachedAt, true);
  writeULongField(file, "    ", "phaseStartedAt", profile.status.phaseStartedAt, true);
  file.print("    \"autoStarted\":"); file.println(profile.status.autoStarted ? "true" : "false");
  file.print(i); file.println("  }");
  file.print(i); file.print("}");
  file.println(trailingComma ? "," : "");
}

float TemperatureProfileEngine::targetForProfile(const BrewProfile& profile, unsigned long nowEpoch,
                                                 TemperatureProfileRuntimePhase* phaseOut) {
  TemperatureProfileRuntimePhase phase = TEMP_PHASE_MAIN;
  float target = profile.temperatureProfile.advanced.mainC;
  if (!profile.temperatureProfile.enabled) {
    float low = rangeLow(profile);
    float high = rangeHigh(profile, low);
    target = (low + high) * 0.5f;
    phase = TEMP_PHASE_NONE;
  } else if ((profile.packageStarted || profile.packageDone) && !profile.packageSkipped) {
    phase = TEMP_PHASE_CARBONATION;
    target = profile.temperatureProfile.advanced.carbonationC;
    if (profile.packageDone && profile.packagedAt > 0 && nowEpoch > profile.packagedAt) {
      unsigned long carbonationSeconds = profile.temperatureProfile.advanced.carbonationDays * 24UL * 3600UL;
      if (nowEpoch >= profile.packagedAt + carbonationSeconds) {
        phase = TEMP_PHASE_STORAGE;
        target = profile.temperatureProfile.advanced.storageC;
      }
    }
  } else if (profile.coldCrashDone && !profile.coldCrashSkipped) {
    phase = TEMP_PHASE_COLD_CRASH;
    target = profile.temperatureProfile.advanced.crashC;
  } else if (profile.dRestDone && !profile.dRestSkipped) {
    phase = TEMP_PHASE_D_REST;
    target = profile.temperatureProfile.advanced.dRestC;
  } else if (profile.fermentationStartAt == 0) {
    phase = TEMP_PHASE_PITCH;
    target = profile.temperatureProfile.advanced.pitchC;
  }
  if (phaseOut) *phaseOut = phase;
  return target;
}

float TemperatureProfileEngine::rampForPhase(const BrewProfile& profile, TemperatureProfileRuntimePhase phase) {
  if (!profile.temperatureProfile.enabled) {
    return phase == TEMP_PHASE_COLD_CRASH ? 1.0f : 0.2f;
  }
  if (phase == TEMP_PHASE_COLD_CRASH) return profile.temperatureProfile.advanced.coldCrashRampKPerH;
  if (phase == TEMP_PHASE_PITCH || phase == TEMP_PHASE_MAIN ||
      phase == TEMP_PHASE_D_REST || phase == TEMP_PHASE_CARBONATION ||
      phase == TEMP_PHASE_STORAGE) {
    return profile.temperatureProfile.advanced.normalRampKPerH;
  }
  return 0.0f;
}

const char* TemperatureProfileEngine::phaseId(TemperatureProfileRuntimePhase phase) {
  switch (phase) {
    case TEMP_PHASE_PITCH: return "pitch";
    case TEMP_PHASE_MAIN: return "main";
    case TEMP_PHASE_D_REST: return "d_rest";
    case TEMP_PHASE_COLD_CRASH: return "cold_crash";
    case TEMP_PHASE_CARBONATION: return "carbonation";
    case TEMP_PHASE_STORAGE: return "storage";
    default: return "legacy";
  }
}

const char* TemperatureProfileEngine::phaseLabel(TemperatureProfileRuntimePhase phase) {
  switch (phase) {
    case TEMP_PHASE_PITCH: return "Pitch";
    case TEMP_PHASE_MAIN: return "Main";
    case TEMP_PHASE_D_REST: return "D-rest";
    case TEMP_PHASE_COLD_CRASH: return "Cold Crash";
    case TEMP_PHASE_CARBONATION: return "Carbonation";
    case TEMP_PHASE_STORAGE: return "Storage";
    default: return "Legacy";
  }
}
