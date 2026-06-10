#ifndef BREW_WIZARD_CONTROLLER_H
#define BREW_WIZARD_CONTROLLER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../domain/brew_profile.h"
#include "../domain/yeast_preset.h"

enum BrewWizardStep {
  WIZARD_BATCH_NAME,
  WIZARD_BEER_STYLE,
  WIZARD_BATCH_SIZE,
  WIZARD_BRIX,
  WIZARD_AUTO_MODE,
  WIZARD_PLUG_CONTROL,
  WIZARD_YEAST,
  WIZARD_ATTENUATION,
  WIZARD_DIACETYL,
  WIZARD_YEAST_BEHAVIOR,
  WIZARD_REVIEW,
  WIZARD_DONE
};

class WizardStepper {
public:
  static void draw(TFT_eSPI& tft, int step, int total, int x, int y, int w);
};

class VirtualKeyboardInput {
public:
  static bool handleTextTouch(int x, int y, char* buffer, size_t bufferSize, bool* clearOnInput = NULL);
  static bool handleNumberTouch(int x, int y, char* buffer, size_t bufferSize, bool* clearOnInput = NULL);
  static void drawText(TFT_eSPI& tft, const char* value);
  static void drawNumber(TFT_eSPI& tft, const char* value);
};

class StyleDropdown {
public:
  static const char* valueAt(int index);
  static int count();
};

class AttenuationDropdown {
public:
  static int valueAt(int index);
  static int count();
};

class IOSSwitch {
public:
  static void draw(TFT_eSPI& tft, int x, int y, bool enabled);
  static bool hit(int x, int y, int switchX, int switchY);
};

class BrewWizardController {
public:
  BrewWizardController();
  void begin(BrewProfile* targetProfile);
  void draw(TFT_eSPI& tft);
  bool handleTouch(int x, int y);
  bool completed() const;
  bool cancelled() const;
  bool cancelConfirmationVisible() const;
  void clearResultFlags();
  BrewWizardStep currentStep() const;

private:
  BrewProfile* profile;
  BrewWizardStep step;
  bool isComplete;
  bool isCancelled;
  bool confirmCancel;
  char editBuffer[40];
  char validationMessage[48];
  bool keyboardUpper;
  bool editPristine;
  char yeastHistory[3][32];
  int yeastHistoryCount;

  void loadStepBuffer();
  void commitStepBuffer();
  bool validateCurrentStep();
  void drawValidationMessage(TFT_eSPI& tft);
  void drawCancelConfirmation(TFT_eSPI& tft);
  void nextStep();
  void previousStep();
  void drawFrame(TFT_eSPI& tft, const char* title);
  void drawNav(TFT_eSPI& tft, bool showBack, bool showNext, const char* nextLabel);
  void drawDropdownSelector(TFT_eSPI& tft, const char* title, const char* selected, const char* hint);
  void drawAttenuationList(TFT_eSPI& tft);
  void drawYeastHistory(TFT_eSPI& tft);
  void drawPresetSummary(TFT_eSPI& tft);
  void applySelectedPreset();
  int currentStyleIndex() const;
  int currentPresetIndex() const;
  void selectStyleOffset(int offset);
  void selectPresetOffset(int offset);
  bool handleCommonNav(int x, int y);
};

#endif
