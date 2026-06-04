#ifndef YEAST_PRESET_REPOSITORY_H
#define YEAST_PRESET_REPOSITORY_H

#include "yeast_preset.h"
#include "brew_profile.h"

class YeastPresetRepository {
public:
  static void invalidateCache();
  static bool reloadCustom();
  static int count();
  static const YeastPreset* at(int index);
  static const YeastPreset* findById(const char* id);
  static const YeastPreset* defaultPreset();
  static void applyToProfile(const YeastPreset& preset, BrewProfile* profile);
  static bool isCustom(const char* id);
  static bool saveCustom(const YeastPreset& preset);
  static bool deleteCustom(const char* id);
};

#endif
