#ifndef TEMPERATURE_PROFILE_H
#define TEMPERATURE_PROFILE_H

#include <Arduino.h>
#include <FS.h>

struct BrewProfile;

#define TEMPERATURE_PROFILE_MAX_PHASES 8

struct TemperatureProfilePhase {
  char id[20];
  char label[28];
  float targetC;
  unsigned long holdHoursAfterReached;
  char triggerType[20];
  char rampClass[16];
  bool requiresActionResolved;
  char autoStartPolicy[20];
};

struct TemperatureProfileAdvanced {
  float pitchC;
  float mainC;
  float dRestC;
  float crashC;
  float lagerC;
  unsigned long mainHoldHours;
  unsigned long dRestHoldHours;
  unsigned long crashHoldHours;
  float targetCO2;
  float carbonationC;
  unsigned long carbonationDays;
  float storageC;
  unsigned long storageDaysHint;
  float normalRampKPerH;
  float coldCrashRampKPerH;
};

struct TemperatureProfileStatus {
  char activePhaseId[20];
  bool targetReached;
  unsigned long targetReachedAt;
  unsigned long phaseStartedAt;
  bool autoStarted;
};

struct TemperatureProfile {
  bool enabled;
  uint8_t version;
  char family[32];
  char presetId[24];
  char source[20];
  TemperatureProfilePhase phases[TEMPERATURE_PROFILE_MAX_PHASES];
  uint8_t phaseCount;
  TemperatureProfileAdvanced advanced;
  TemperatureProfileStatus status;
};

enum TemperatureProfileRuntimePhase {
  TEMP_PHASE_NONE = 0,
  TEMP_PHASE_PITCH,
  TEMP_PHASE_MAIN,
  TEMP_PHASE_D_REST,
  TEMP_PHASE_COLD_CRASH,
  TEMP_PHASE_CARBONATION,
  TEMP_PHASE_STORAGE
};

class TemperatureProfileEngine {
public:
  static void setDefaults(TemperatureProfile* profile);
  static void generateForProfile(BrewProfile* profile);
  static bool loadFromJson(const char* json, TemperatureProfile* profile);
  static void writeJson(fs::File& file, const TemperatureProfile& profile, const char* indent, bool trailingComma);
  static void rebuildPhaseList(TemperatureProfile* profile);
  static float targetForProfile(const BrewProfile& profile, unsigned long nowEpoch,
                                TemperatureProfileRuntimePhase* phaseOut = NULL);
  static float rampForPhase(const BrewProfile& profile, TemperatureProfileRuntimePhase phase);
  static const char* phaseId(TemperatureProfileRuntimePhase phase);
  static const char* phaseLabel(TemperatureProfileRuntimePhase phase);
};

#endif
