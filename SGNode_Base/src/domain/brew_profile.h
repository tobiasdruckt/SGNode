#ifndef BREW_PROFILE_H
#define BREW_PROFILE_H

#include <Arduino.h>
#include <FS.h>
#include "temperature_profile.h"

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
  unsigned long fermentationStartAt;
  bool ogVerified;
  bool ogNeedsChoice;
  bool autoModeEnabled;
  bool plugControlEnabled;
  bool floatZeroCalDone;
  bool floatZeroCalSkipped;
  unsigned long floatZeroCalibratedAt;
  bool floatInBrewConfirmed;
  unsigned long floatInBrewAt;
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
  bool completed;
  unsigned long completedAt;
  bool dryHopEnabled;
  float dryHopFgOffset;
  float dryHopTriggerSG;
  unsigned long dryHopContactHours;
  bool dryHopDone;
  bool dryHopSkipped;
  unsigned long dryHopStartTime;
  bool dryHopRemoved;
  bool dryHopRemoveSkipped;
  unsigned long dryHopRemovedAt;
  bool dRestDone;
  bool dRestSkipped;
  unsigned long dRestStartedAt;
  bool coldCrashDone;
  bool coldCrashSkipped;
  unsigned long coldCrashStartedAt;
  bool packageStarted;
  unsigned long packageStartedAt;
  bool packageDone;
  bool packageSkipped;
  unsigned long packagedAt;
  TemperatureProfile temperatureProfile;
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
  static void eventsPath(const char* batchId, char* buffer, size_t bufferSize);
  static void activeBatchPath(char* buffer, size_t bufferSize);
  static bool ensureBatchDirectory(const char* batchId);
  static bool save(const BrewProfile& profile);
  static bool load(const char* batchId, BrewProfile* profile);
  static bool loadFromPath(const char* path, BrewProfile* profile);
  static bool saveActiveBatchId(const char* batchId);
  static bool loadActiveBatchId(char* batchId, size_t bufferSize);
  static bool clearActiveBatchId();
  static void saveYeastHistory(const char* yeastName);
  static int loadYeastHistory(char names[][32], int maxNames);
  static bool appendYeastPerformance(const YeastPerformanceSummary& summary);
  static bool appendBatchEvent(const char* batchId, unsigned long epoch, const char* eventType,
                               const char* message, float value);
};

#endif
