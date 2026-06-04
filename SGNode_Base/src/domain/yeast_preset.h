#ifndef YEAST_PRESET_H
#define YEAST_PRESET_H

struct YeastPreset {
  const char* id;
  const char* displayName;
  const char* category;
  int typicalAttenuationMin;
  int typicalAttenuationMax;
  int defaultAttenuation;
  const char* fermentationSpeed;
  float lagPhaseHours;
  float typicalDurationHours;
  float recommendedTempMinC;
  float recommendedTempMaxC;
  const char* flocculation;
  const char* curveTemplate;
  bool diacetylRestRecommended;
  const char* notes;
};

#endif
