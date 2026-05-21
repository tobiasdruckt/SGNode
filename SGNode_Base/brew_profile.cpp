#include "brew_profile.h"
#include <SD.h>
#include <string.h>
#include <stdlib.h>

static void jsonEscapePrint(File& file, const char* text) {
  for (const char* p = text; *p; ++p) {
    if (*p == '"' || *p == '\\') file.print('\\');
    if ((unsigned char)*p >= 32) file.print(*p);
  }
}

static bool extractString(const char* json, const char* key, char* out, size_t outSize) {
  char pattern[32];
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
  char pattern[32];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* start = strstr(json, pattern);
  if (!start) return fallback;
  return atof(start + strlen(pattern));
}

static int extractInt(const char* json, const char* key, int fallback) {
  return (int)extractFloat(json, key, fallback);
}

static unsigned long extractULong(const char* json, const char* key, unsigned long fallback) {
  char pattern[32];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* start = strstr(json, pattern);
  if (!start) return fallback;
  return strtoul(start + strlen(pattern), NULL, 10);
}

static bool extractBool(const char* json, const char* key, bool fallback) {
  char pattern[32];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* start = strstr(json, pattern);
  if (!start) return fallback;
  start += strlen(pattern);
  return strncmp(start, "true", 4) == 0;
}

void BrewProfileStore::setDefaults(BrewProfile* profile) {
  memset(profile, 0, sizeof(BrewProfile));
  strcpy(profile->batchId, "batch_001");
  strcpy(profile->batchName, "New Batch");
  strcpy(profile->beerStyle, "German Pils");
  profile->batchSizeLiters = 20.0f;
  profile->recipeBrix = 12.0f;
  profile->recipeOG = 1.048f;
  profile->measuredOG = 0.0f;
  profile->effectiveOG = profile->recipeOG;
  profile->ogDifference = 0.0f;
  strcpy(profile->yeastName, "House Yeast");
  profile->expectedApparentAttenuation = 75;
  profile->expectedFinalGravity = 1.012f;
  profile->estimatedABV = 0.0f;
  profile->diacetylRestEnabled = false;
  profile->createdAt = 0;
  profile->ogVerified = false;
  profile->ogNeedsChoice = false;
  profile->autoModeEnabled = true;
  strcpy(profile->selectedYeastPresetId, "us05");
  strcpy(profile->selectedYeastPresetName, "US-05");
  strcpy(profile->yeastCategory, "American Ale");
  profile->yeastDefaultAttenuation = 80;
  profile->yeastAttenuationMin = 78;
  profile->yeastAttenuationMax = 82;
  strcpy(profile->fermentationSpeed, "medium-fast");
  profile->lagPhaseHours = 9.0f;
  profile->typicalDurationHours = 132.0f;
  profile->recommendedTempMinC = 18.0f;
  profile->recommendedTempMaxC = 22.0f;
  strcpy(profile->curveTemplate, "clean_ale");
  profile->diacetylRestRecommendedByYeast = false;
  strcpy(profile->attenuationSource, "yeast_preset");
}

void BrewProfileStore::buildBatchId(int number, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "batch_%03d", number);
}

void BrewProfileStore::profilePath(const char* batchId, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "/data/batches/%s/profile.json", batchId);
}

void BrewProfileStore::targetPath(const char* batchId, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "/data/batches/%s/target.json", batchId);
}

void BrewProfileStore::logPath(const char* batchId, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "/data/batches/%s/log.csv", batchId);
}

bool BrewProfileStore::ensureBatchDirectory(const char* batchId) {
  if (!SD.exists("/data")) SD.mkdir("/data");
  if (!SD.exists("/data/batches")) SD.mkdir("/data/batches");
  char dir[64];
  snprintf(dir, sizeof(dir), "/data/batches/%s", batchId);
  if (!SD.exists(dir)) return SD.mkdir(dir);
  return true;
}

bool BrewProfileStore::save(const BrewProfile& profile) {
  if (!ensureBatchDirectory(profile.batchId)) return false;
  char path[80];
  profilePath(profile.batchId, path, sizeof(path));
  if (SD.exists(path)) SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;

  file.println("{");
  file.print("  \"batchId\":\""); jsonEscapePrint(file, profile.batchId); file.println("\",");
  file.print("  \"batchName\":\""); jsonEscapePrint(file, profile.batchName); file.println("\",");
  file.print("  \"beerStyle\":\""); jsonEscapePrint(file, profile.beerStyle); file.println("\",");
  file.printf("  \"batchSizeLiters\":%.2f,\n", profile.batchSizeLiters);
  file.printf("  \"recipeBrix\":%.2f,\n", profile.recipeBrix);
  file.printf("  \"recipeOG\":%.5f,\n", profile.recipeOG);
  file.printf("  \"measuredOG\":%.5f,\n", profile.measuredOG);
  file.printf("  \"effectiveOG\":%.5f,\n", profile.effectiveOG);
  file.printf("  \"ogDifference\":%.5f,\n", profile.ogDifference);
  file.print("  \"yeastName\":\""); jsonEscapePrint(file, profile.yeastName); file.println("\",");
  file.printf("  \"expectedApparentAttenuation\":%d,\n", profile.expectedApparentAttenuation);
  file.printf("  \"expectedFinalGravity\":%.5f,\n", profile.expectedFinalGravity);
  file.printf("  \"estimatedABV\":%.2f,\n", profile.estimatedABV);
  file.printf("  \"diacetylRestEnabled\":%s,\n", profile.diacetylRestEnabled ? "true" : "false");
  file.printf("  \"createdAt\":%lu,\n", profile.createdAt);
  file.printf("  \"ogVerified\":%s,\n", profile.ogVerified ? "true" : "false");
  file.printf("  \"ogNeedsChoice\":%s,\n", profile.ogNeedsChoice ? "true" : "false");
  file.printf("  \"autoModeEnabled\":%s,\n", profile.autoModeEnabled ? "true" : "false");
  file.print("  \"selectedYeastPresetId\":\""); jsonEscapePrint(file, profile.selectedYeastPresetId); file.println("\",");
  file.print("  \"selectedYeastPresetName\":\""); jsonEscapePrint(file, profile.selectedYeastPresetName); file.println("\",");
  file.print("  \"yeastCategory\":\""); jsonEscapePrint(file, profile.yeastCategory); file.println("\",");
  file.printf("  \"yeastDefaultAttenuation\":%d,\n", profile.yeastDefaultAttenuation);
  file.printf("  \"yeastAttenuationMin\":%d,\n", profile.yeastAttenuationMin);
  file.printf("  \"yeastAttenuationMax\":%d,\n", profile.yeastAttenuationMax);
  file.print("  \"fermentationSpeed\":\""); jsonEscapePrint(file, profile.fermentationSpeed); file.println("\",");
  file.printf("  \"lagPhaseHours\":%.1f,\n", profile.lagPhaseHours);
  file.printf("  \"typicalDurationHours\":%.1f,\n", profile.typicalDurationHours);
  file.printf("  \"recommendedTempMinC\":%.1f,\n", profile.recommendedTempMinC);
  file.printf("  \"recommendedTempMaxC\":%.1f,\n", profile.recommendedTempMaxC);
  file.print("  \"curveTemplate\":\""); jsonEscapePrint(file, profile.curveTemplate); file.println("\",");
  file.printf("  \"diacetylRestRecommendedByYeast\":%s,\n", profile.diacetylRestRecommendedByYeast ? "true" : "false");
  file.print("  \"attenuationSource\":\""); jsonEscapePrint(file, profile.attenuationSource); file.println("\"");
  file.println("}");
  file.close();
  return true;
}

bool BrewProfileStore::load(const char* batchId, BrewProfile* profile) {
  char path[80];
  profilePath(batchId, path, sizeof(path));
  return loadFromPath(path, profile);
}

bool BrewProfileStore::loadFromPath(const char* path, BrewProfile* profile) {
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  size_t size = file.size();
  if (size > 4095) size = 4095;
  char json[4096];
  size_t read = file.readBytes(json, size);
  json[read] = '\0';
  file.close();

  setDefaults(profile);
  extractString(json, "batchId", profile->batchId, sizeof(profile->batchId));
  extractString(json, "batchName", profile->batchName, sizeof(profile->batchName));
  extractString(json, "beerStyle", profile->beerStyle, sizeof(profile->beerStyle));
  profile->batchSizeLiters = extractFloat(json, "batchSizeLiters", profile->batchSizeLiters);
  profile->recipeBrix = extractFloat(json, "recipeBrix", profile->recipeBrix);
  profile->recipeOG = extractFloat(json, "recipeOG", profile->recipeOG);
  profile->measuredOG = extractFloat(json, "measuredOG", profile->measuredOG);
  profile->effectiveOG = extractFloat(json, "effectiveOG", profile->effectiveOG);
  profile->ogDifference = extractFloat(json, "ogDifference", profile->ogDifference);
  extractString(json, "yeastName", profile->yeastName, sizeof(profile->yeastName));
  profile->expectedApparentAttenuation = extractInt(json, "expectedApparentAttenuation", profile->expectedApparentAttenuation);
  profile->expectedFinalGravity = extractFloat(json, "expectedFinalGravity", profile->expectedFinalGravity);
  profile->estimatedABV = extractFloat(json, "estimatedABV", profile->estimatedABV);
  profile->diacetylRestEnabled = extractBool(json, "diacetylRestEnabled", profile->diacetylRestEnabled);
  profile->createdAt = extractULong(json, "createdAt", profile->createdAt);
  profile->ogVerified = extractBool(json, "ogVerified", profile->ogVerified);
  profile->ogNeedsChoice = extractBool(json, "ogNeedsChoice", profile->ogNeedsChoice);
  profile->autoModeEnabled = extractBool(json, "autoModeEnabled", profile->autoModeEnabled);
  extractString(json, "selectedYeastPresetId", profile->selectedYeastPresetId, sizeof(profile->selectedYeastPresetId));
  extractString(json, "selectedYeastPresetName", profile->selectedYeastPresetName, sizeof(profile->selectedYeastPresetName));
  extractString(json, "yeastCategory", profile->yeastCategory, sizeof(profile->yeastCategory));
  profile->yeastDefaultAttenuation = extractInt(json, "yeastDefaultAttenuation", profile->yeastDefaultAttenuation);
  profile->yeastAttenuationMin = extractInt(json, "yeastAttenuationMin", profile->yeastAttenuationMin);
  profile->yeastAttenuationMax = extractInt(json, "yeastAttenuationMax", profile->yeastAttenuationMax);
  extractString(json, "fermentationSpeed", profile->fermentationSpeed, sizeof(profile->fermentationSpeed));
  profile->lagPhaseHours = extractFloat(json, "lagPhaseHours", profile->lagPhaseHours);
  profile->typicalDurationHours = extractFloat(json, "typicalDurationHours", profile->typicalDurationHours);
  profile->recommendedTempMinC = extractFloat(json, "recommendedTempMinC", profile->recommendedTempMinC);
  profile->recommendedTempMaxC = extractFloat(json, "recommendedTempMaxC", profile->recommendedTempMaxC);
  extractString(json, "curveTemplate", profile->curveTemplate, sizeof(profile->curveTemplate));
  profile->diacetylRestRecommendedByYeast = extractBool(json, "diacetylRestRecommendedByYeast", profile->diacetylRestRecommendedByYeast);
  extractString(json, "attenuationSource", profile->attenuationSource, sizeof(profile->attenuationSource));
  return true;
}

void BrewProfileStore::saveYeastHistory(const char* yeastName) {
  if (yeastName == NULL || yeastName[0] == '\0') return;
  if (!SD.exists("/data")) SD.mkdir("/data");
  File file = SD.open("/data/yeasts.txt", FILE_APPEND);
  if (!file) return;
  file.println(yeastName);
  file.close();
}

int BrewProfileStore::loadYeastHistory(char names[][32], int maxNames) {
  File file = SD.open("/data/yeasts.txt", FILE_READ);
  if (!file) return 0;
  int count = 0;
  char line[40];
  while (file.available() && count < maxNames) {
    int bytes = file.readBytesUntil('\n', line, sizeof(line) - 1);
    line[bytes] = '\0';
    char* cr = strchr(line, '\r');
    if (cr) *cr = '\0';
    if (line[0] != '\0') {
      strncpy(names[count], line, 31);
      names[count][31] = '\0';
      count++;
    }
  }
  file.close();
  return count;
}

bool BrewProfileStore::appendYeastPerformance(const YeastPerformanceSummary& summary) {
  if (!SD.exists("/data")) SD.mkdir("/data");
  char existing[3072];
  size_t existingLen = 0;
  if (SD.exists("/data/yeast_history.json")) {
    File oldFile = SD.open("/data/yeast_history.json", FILE_READ);
    if (oldFile) {
      existingLen = oldFile.readBytes(existing, sizeof(existing) - 1);
      existing[existingLen] = '\0';
      oldFile.close();
    }
  }

  while (existingLen > 0 && (existing[existingLen - 1] == '\n' || existing[existingLen - 1] == '\r' ||
                             existing[existingLen - 1] == ' ' || existing[existingLen - 1] == '\t')) {
    existing[--existingLen] = '\0';
  }
  bool hasExistingObject = existingLen > 2;
  if (hasExistingObject && existing[existingLen - 1] == ']') {
    existing[--existingLen] = '\0';
  }

  if (SD.exists("/data/yeast_history.json")) SD.remove("/data/yeast_history.json");
  File file = SD.open("/data/yeast_history.json", FILE_WRITE);
  if (!file) return false;
  if (hasExistingObject) {
    file.print(existing);
    file.println(",");
  } else {
    file.println("[");
  }
  file.print("  {\"yeastPresetId\":\""); jsonEscapePrint(file, summary.yeastPresetId);
  file.print("\",\"yeastName\":\""); jsonEscapePrint(file, summary.yeastName);
  file.print("\",\"batchId\":\""); jsonEscapePrint(file, summary.batchId);
  file.printf("\",\"effectiveOG\":%.5f,\"finalGravity\":%.5f,\"actualAttenuation\":%.2f,",
              summary.effectiveOG, summary.finalGravity, summary.actualAttenuation);
  file.printf("\"fermentationDurationHours\":%.1f,\"averageTemperature\":%.2f,\"completedAt\":%lu}\n",
              summary.fermentationDurationHours, summary.averageTemperature, summary.completedAt);
  file.println("]");
  file.close();
  return true;
}
