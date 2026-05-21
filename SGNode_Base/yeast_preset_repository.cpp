#include "yeast_preset_repository.h"
#include "derived_calculations.h"
#include <SD.h>
#include <string.h>
#include <stdlib.h>

static const YeastPreset YEAST_PRESETS[] = {
  {"us05", "US-05", "American Ale", 78, 82, 80, "medium-fast", 9.0f, 132.0f, 18.0f, 22.0f, "medium", "clean_ale", false, "Clean American ale profile"},
  {"s04", "S-04", "English Ale", 74, 78, 76, "fast", 7.0f, 96.0f, 18.0f, 22.0f, "high", "fast_ale", false, "Fast English ale behavior"},
  {"nottingham", "Nottingham", "Ale", 77, 83, 80, "medium-fast", 9.0f, 120.0f, 14.0f, 22.0f, "high", "clean_ale", false, "Versatile clean ale yeast"},
  {"k97", "K-97", "Kolsch / German Ale", 78, 82, 80, "medium", 12.0f, 156.0f, 15.0f, 20.0f, "medium", "slow_clean_ale", true, "Clean German ale behavior"},
  {"w3470", "W-34/70", "Lager", 80, 84, 82, "slow", 18.0f, 252.0f, 9.0f, 15.0f, "medium", "lager", true, "Classic lager fermentation"},
  {"s23", "S-23", "Lager", 78, 82, 80, "slow", 18.0f, 252.0f, 10.0f, 15.0f, "medium", "lager", true, "Lager profile with rest guidance"},
  {"voss", "Voss Kveik", "Kveik", 76, 82, 79, "very_fast", 4.0f, 66.0f, 25.0f, 40.0f, "medium", "very_fast_ale", false, "Very fast warm fermentation"},
  {"belle_saison", "Belle Saison", "Saison", 85, 95, 90, "medium", 12.0f, 180.0f, 20.0f, 30.0f, "low", "high_attenuation_saison", false, "High attenuation saison profile"},
  {"generic_ale", "Generic Ale", "Generic", 74, 80, 77, "medium", 9.0f, 132.0f, 18.0f, 22.0f, "medium", "clean_ale", false, "Default ale behavior"},
  {"generic_lager", "Generic Lager", "Generic", 78, 84, 81, "slow", 18.0f, 252.0f, 9.0f, 15.0f, "medium", "lager", true, "Default lager behavior"}
};

struct StoredYeastPreset {
  char id[24];
  char displayName[32];
  char category[32];
  int typicalAttenuationMin;
  int typicalAttenuationMax;
  int defaultAttenuation;
  char fermentationSpeed[16];
  float lagPhaseHours;
  float typicalDurationHours;
  float recommendedTempMinC;
  float recommendedTempMaxC;
  char flocculation[16];
  char curveTemplate[32];
  bool diacetylRestRecommended;
  char notes[64];
};

#define MAX_CUSTOM_YEASTS 12
static StoredYeastPreset CUSTOM_YEASTS[MAX_CUSTOM_YEASTS];
static YeastPreset CUSTOM_VIEW[MAX_CUSTOM_YEASTS];
static int customYeastCount = 0;
static bool customLoaded = false;

static void jsonEscapePrint(File& file, const char* text) {
  for (const char* p = text; *p; ++p) {
    if (*p == '"' || *p == '\\') file.print('\\');
    if ((unsigned char)*p >= 32) file.print(*p);
  }
}

static bool extractString(const char* json, const char* key, char* out, size_t outSize) {
  char pattern[36];
  snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
  const char* start = strstr(json, pattern);
  if (!start) return false;
  start += strlen(pattern);
  const char* end = strchr(start, '"');
  if (!end) return false;
  size_t len = end - start;
  if (len >= outSize) len = outSize - 1;
  strncpy(out, start, len);
  out[len] = '\0';
  return true;
}

static float extractFloat(const char* json, const char* key, float fallback) {
  char pattern[36];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* start = strstr(json, pattern);
  if (!start) return fallback;
  return atof(start + strlen(pattern));
}

static int extractInt(const char* json, const char* key, int fallback) {
  return (int)extractFloat(json, key, fallback);
}

static bool extractBool(const char* json, const char* key, bool fallback) {
  char pattern[36];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* start = strstr(json, pattern);
  if (!start) return fallback;
  start += strlen(pattern);
  return strncmp(start, "true", 4) == 0;
}

static void sanitizeId(const char* name, char* id, size_t idSize) {
  snprintf(id, idSize, "custom_");
  size_t pos = strlen(id);
  for (const char* p = name; *p && pos + 1 < idSize; ++p) {
    char c = *p;
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      id[pos++] = c;
    } else if (pos > 7 && id[pos - 1] != '_') {
      id[pos++] = '_';
    }
  }
  if (pos <= 7 && pos + 5 < idSize) {
    strcpy(id + pos, "yeast");
  } else {
    id[pos] = '\0';
  }
}

static void bindCustomView(int index) {
  StoredYeastPreset& s = CUSTOM_YEASTS[index];
  CUSTOM_VIEW[index] = {
    s.id, s.displayName, s.category,
    s.typicalAttenuationMin, s.typicalAttenuationMax, s.defaultAttenuation,
    s.fermentationSpeed, s.lagPhaseHours, s.typicalDurationHours,
    s.recommendedTempMinC, s.recommendedTempMaxC,
    s.flocculation, s.curveTemplate, s.diacetylRestRecommended, s.notes
  };
}

static bool parseCustomPresetFile(const char* path, StoredYeastPreset* preset) {
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  char json[1024];
  size_t len = file.readBytes(json, sizeof(json) - 1);
  json[len] = '\0';
  file.close();

  memset(preset, 0, sizeof(StoredYeastPreset));
  extractString(json, "id", preset->id, sizeof(preset->id));
  extractString(json, "displayName", preset->displayName, sizeof(preset->displayName));
  extractString(json, "category", preset->category, sizeof(preset->category));
  preset->typicalAttenuationMin = extractInt(json, "typicalAttenuationMin", 74);
  preset->typicalAttenuationMax = extractInt(json, "typicalAttenuationMax", 80);
  preset->defaultAttenuation = extractInt(json, "defaultAttenuation", 77);
  extractString(json, "fermentationSpeed", preset->fermentationSpeed, sizeof(preset->fermentationSpeed));
  preset->lagPhaseHours = extractFloat(json, "lagPhaseHours", 9.0f);
  preset->typicalDurationHours = extractFloat(json, "typicalDurationHours", 132.0f);
  preset->recommendedTempMinC = extractFloat(json, "recommendedTempMinC", 18.0f);
  preset->recommendedTempMaxC = extractFloat(json, "recommendedTempMaxC", 22.0f);
  extractString(json, "flocculation", preset->flocculation, sizeof(preset->flocculation));
  extractString(json, "curveTemplate", preset->curveTemplate, sizeof(preset->curveTemplate));
  preset->diacetylRestRecommended = extractBool(json, "diacetylRestRecommended", false);
  extractString(json, "notes", preset->notes, sizeof(preset->notes));

  if (preset->displayName[0] == '\0') return false;
  if (preset->id[0] == '\0') sanitizeId(preset->displayName, preset->id, sizeof(preset->id));
  if (preset->category[0] == '\0') strcpy(preset->category, "Custom");
  if (preset->fermentationSpeed[0] == '\0') strcpy(preset->fermentationSpeed, "medium");
  if (preset->flocculation[0] == '\0') strcpy(preset->flocculation, "medium");
  if (preset->curveTemplate[0] == '\0') strcpy(preset->curveTemplate, "clean_ale");
  return true;
}

void YeastPresetRepository::invalidateCache() {
  customLoaded = false;
}

bool YeastPresetRepository::reloadCustom() {
  customYeastCount = 0;
  customLoaded = true;
  if (!SD.exists("/data")) SD.mkdir("/data");
  if (!SD.exists("/data/yeast")) SD.mkdir("/data/yeast");
  File dir = SD.open("/data/yeast");
  if (!dir) return false;
  File entry = dir.openNextFile();
  while (entry && customYeastCount < MAX_CUSTOM_YEASTS) {
    if (!entry.isDirectory()) {
      char path[64];
      const char* name = entry.name();
      const char* slash = strrchr(name, '/');
      snprintf(path, sizeof(path), "/data/yeast/%s", slash ? slash + 1 : name);
      if (parseCustomPresetFile(path, &CUSTOM_YEASTS[customYeastCount])) {
        bindCustomView(customYeastCount);
        customYeastCount++;
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  return true;
}

int YeastPresetRepository::count() {
  if (!customLoaded) reloadCustom();
  return (sizeof(YEAST_PRESETS) / sizeof(YEAST_PRESETS[0])) + customYeastCount;
}

const YeastPreset* YeastPresetRepository::at(int index) {
  if (!customLoaded) reloadCustom();
  int builtInCount = sizeof(YEAST_PRESETS) / sizeof(YEAST_PRESETS[0]);
  if (index < 0 || index >= builtInCount + customYeastCount) return defaultPreset();
  if (index < builtInCount) return &YEAST_PRESETS[index];
  return &CUSTOM_VIEW[index - builtInCount];
}

const YeastPreset* YeastPresetRepository::findById(const char* id) {
  if (id == NULL || id[0] == '\0') return defaultPreset();
  if (!customLoaded) reloadCustom();
  int builtInCount = sizeof(YEAST_PRESETS) / sizeof(YEAST_PRESETS[0]);
  for (int i = 0; i < builtInCount; i++) {
    if (strcmp(YEAST_PRESETS[i].id, id) == 0) return &YEAST_PRESETS[i];
  }
  for (int i = 0; i < customYeastCount; i++) {
    if (strcmp(CUSTOM_YEASTS[i].id, id) == 0) return &CUSTOM_VIEW[i];
  }
  return defaultPreset();
}

const YeastPreset* YeastPresetRepository::defaultPreset() {
  return &YEAST_PRESETS[0];
}

void YeastPresetRepository::applyToProfile(const YeastPreset& preset, BrewProfile* profile) {
  if (!profile) return;
  profile->autoModeEnabled = true;
  strncpy(profile->selectedYeastPresetId, preset.id, sizeof(profile->selectedYeastPresetId) - 1);
  strncpy(profile->selectedYeastPresetName, preset.displayName, sizeof(profile->selectedYeastPresetName) - 1);
  strncpy(profile->yeastName, preset.displayName, sizeof(profile->yeastName) - 1);
  strncpy(profile->yeastCategory, preset.category, sizeof(profile->yeastCategory) - 1);
  profile->yeastDefaultAttenuation = preset.defaultAttenuation;
  profile->yeastAttenuationMin = preset.typicalAttenuationMin;
  profile->yeastAttenuationMax = preset.typicalAttenuationMax;
  profile->expectedApparentAttenuation = preset.defaultAttenuation;
  strncpy(profile->fermentationSpeed, preset.fermentationSpeed, sizeof(profile->fermentationSpeed) - 1);
  profile->lagPhaseHours = preset.lagPhaseHours;
  profile->typicalDurationHours = preset.typicalDurationHours;
  profile->recommendedTempMinC = preset.recommendedTempMinC;
  profile->recommendedTempMaxC = preset.recommendedTempMaxC;
  strncpy(profile->curveTemplate, preset.curveTemplate, sizeof(profile->curveTemplate) - 1);
  profile->diacetylRestRecommendedByYeast = preset.diacetylRestRecommended;
  profile->diacetylRestEnabled = preset.diacetylRestRecommended;
  strcpy(profile->attenuationSource, "yeast_preset");
  profile->expectedFinalGravity = DerivedCalculations::expectedFG(profile->effectiveOG, profile->expectedApparentAttenuation);

  profile->selectedYeastPresetId[sizeof(profile->selectedYeastPresetId) - 1] = '\0';
  profile->selectedYeastPresetName[sizeof(profile->selectedYeastPresetName) - 1] = '\0';
  profile->yeastName[sizeof(profile->yeastName) - 1] = '\0';
  profile->yeastCategory[sizeof(profile->yeastCategory) - 1] = '\0';
  profile->fermentationSpeed[sizeof(profile->fermentationSpeed) - 1] = '\0';
  profile->curveTemplate[sizeof(profile->curveTemplate) - 1] = '\0';
  profile->attenuationSource[sizeof(profile->attenuationSource) - 1] = '\0';
}

bool YeastPresetRepository::isCustom(const char* id) {
  return id && strncmp(id, "custom_", 7) == 0;
}

bool YeastPresetRepository::saveCustom(const YeastPreset& preset) {
  if (!SD.exists("/data")) SD.mkdir("/data");
  if (!SD.exists("/data/yeast")) SD.mkdir("/data/yeast");
  char id[24];
  if (preset.id && preset.id[0]) strncpy(id, preset.id, sizeof(id) - 1);
  else sanitizeId(preset.displayName, id, sizeof(id));
  id[sizeof(id) - 1] = '\0';
  if (strncmp(id, "custom_", 7) != 0) {
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "custom_%s", id);
    strncpy(id, tmp, sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
  }
  char path[64];
  snprintf(path, sizeof(path), "/data/yeast/%s.json", id);
  if (SD.exists(path)) SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;
  file.println("{");
  file.print("  \"id\":\""); jsonEscapePrint(file, id); file.println("\",");
  file.print("  \"displayName\":\""); jsonEscapePrint(file, preset.displayName); file.println("\",");
  file.print("  \"category\":\""); jsonEscapePrint(file, preset.category); file.println("\",");
  file.printf("  \"typicalAttenuationMin\":%d,\n", preset.typicalAttenuationMin);
  file.printf("  \"typicalAttenuationMax\":%d,\n", preset.typicalAttenuationMax);
  file.printf("  \"defaultAttenuation\":%d,\n", preset.defaultAttenuation);
  file.print("  \"fermentationSpeed\":\""); jsonEscapePrint(file, preset.fermentationSpeed); file.println("\",");
  file.printf("  \"lagPhaseHours\":%.1f,\n", preset.lagPhaseHours);
  file.printf("  \"typicalDurationHours\":%.1f,\n", preset.typicalDurationHours);
  file.printf("  \"recommendedTempMinC\":%.1f,\n", preset.recommendedTempMinC);
  file.printf("  \"recommendedTempMaxC\":%.1f,\n", preset.recommendedTempMaxC);
  file.print("  \"flocculation\":\""); jsonEscapePrint(file, preset.flocculation); file.println("\",");
  file.print("  \"curveTemplate\":\""); jsonEscapePrint(file, preset.curveTemplate); file.println("\",");
  file.printf("  \"diacetylRestRecommended\":%s,\n", preset.diacetylRestRecommended ? "true" : "false");
  file.print("  \"notes\":\""); jsonEscapePrint(file, preset.notes); file.println("\"");
  file.println("}");
  file.close();
  invalidateCache();
  return true;
}

bool YeastPresetRepository::deleteCustom(const char* id) {
  if (!isCustom(id)) return false;
  char path[64];
  snprintf(path, sizeof(path), "/data/yeast/%s.json", id);
  if (!SD.exists(path)) return false;
  bool ok = SD.remove(path);
  invalidateCache();
  return ok;
}
