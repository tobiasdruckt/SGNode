#ifndef BREW_PROFILE_H
#define BREW_PROFILE_H

#include <Arduino.h>
#include <FS.h>

struct BrewProfile {
  char batchId[24];
  char batchName[40];
  char beerStyle[24];
  float batchSizeLiters;
  float recipeBrix;
  float recipeOG;
  float measuredOG;
  float effectiveOG;
  float ogDifference;
  char yeastName[32];
  int expectedApparentAttenuation;
  float expectedFinalGravity;
  float estimatedABV;
  bool diacetylRestEnabled;
  unsigned long createdAt;
  bool ogVerified;
  bool ogNeedsChoice;
  bool autoModeEnabled;
  char selectedYeastPresetId[24];
  char selectedYeastPresetName[32];
  char yeastCategory[32];
  int yeastDefaultAttenuation;
  int yeastAttenuationMin;
  int yeastAttenuationMax;
  char fermentationSpeed[16];
  float lagPhaseHours;
  float typicalDurationHours;
  float recommendedTempMinC;
  float recommendedTempMaxC;
  char curveTemplate[32];
  bool diacetylRestRecommendedByYeast;
  char attenuationSource[18];
};

struct YeastPerformanceSummary {
  char yeastPresetId[24];
  char yeastName[32];
  char batchId[24];
  float effectiveOG;
  float finalGravity;
  float actualAttenuation;
  float fermentationDurationHours;
  float averageTemperature;
  unsigned long completedAt;
};

class BrewProfileStore {
public:
  static void setDefaults(BrewProfile* profile);
  static void buildBatchId(int number, char* buffer, size_t bufferSize);
  static void profilePath(const char* batchId, char* buffer, size_t bufferSize);
  static void targetPath(const char* batchId, char* buffer, size_t bufferSize);
  static void logPath(const char* batchId, char* buffer, size_t bufferSize);
  static bool ensureBatchDirectory(const char* batchId);
  static bool save(const BrewProfile& profile);
  static bool load(const char* batchId, BrewProfile* profile);
  static bool loadFromPath(const char* path, BrewProfile* profile);
  static void saveYeastHistory(const char* yeastName);
  static int loadYeastHistory(char names[][32], int maxNames);
  static bool appendYeastPerformance(const YeastPerformanceSummary& summary);
};

#endif
