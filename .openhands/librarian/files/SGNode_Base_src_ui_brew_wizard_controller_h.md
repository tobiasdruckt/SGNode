# SGNode_Base/src/ui/brew_wizard_controller.h

**Type:** C++ Header
**Hash:** `e6a99a71c857fba0a83d92e6623486c5f1bd6fabed9310bd792dc3acadfcc989`

## Overview / Role

Control logic for temperature or other systems

## Verified API & Symbols

### Structs & Classes

- `class WizardStepper`
  - void draw (TFT_eSPI& tft, int step, int total, int x, int y, int w) static

- `class VirtualKeyboardInput`
  - bool handleTextTouch (int x, int y, char* buffer, size_t bufferSize, bool* clearOnInput = NULL) static
  - bool handleNumberTouch (int x, int y, char* buffer, size_t bufferSize, bool* clearOnInput = NULL) static
  - void drawText (TFT_eSPI& tft, const char* value) static
  - void drawNumber (TFT_eSPI& tft, const char* value) static

- `class StyleDropdown`
  - const char * valueAt (int index) const static
  - int count (void) static

- `class AttenuationDropdown`
  - int valueAt (int index) static
  - int count (void) static

- `class IOSSwitch`
  - void draw (TFT_eSPI& tft, int x, int y, bool enabled) static
  - bool hit (int x, int y, int switchX, int switchY) static

- `class BrewWizardController`
  - BrewWizardControlle r (void)
  - void begin (BrewProfile* targetProfile)
  - void draw (TFT_eSPI& tft)
  - bool handleTouch (int x, int y)
  - bool completed (void)
  - bool cancelled (void)
  - bool cancelConfirmationVisible (void)
  - void clearResultFlags (void)
  - BrewWizardStep currentStep (void)
  - [private]
  - void loadStepBuffer (void)
  - void commitStepBuffer (void)
  - bool validateCurrentStep (void)
  - void drawValidationMessage (TFT_eSPI& tft)
  - void drawCancelConfirmation (TFT_eSPI& tft)
  - void nextStep (void)
  - void previousStep (void)
  - void drawFrame (TFT_eSPI& tft, const char* title)
  - void drawNav (TFT_eSPI& tft, bool showBack, bool showNext, const char* nextLabel)
  - void drawDropdownSelector (TFT_eSPI& tft, const char* title, const char* selected, const char* hint)
  - void drawAttenuationList (TFT_eSPI& tft)
  - void drawYeastHistory (TFT_eSPI& tft)
  - void drawPresetSummary (TFT_eSPI& tft)
  - void drawTemperatureAdvanced (TFT_eSPI& tft)
  - bool handleTemperatureAdvancedTouch (int x, int y)
  - void syncBrewProfileDerivedFields (void)
  - void applySelectedPreset (void)
  - int currentStyleIndex (void)
  - int currentPresetIndex (void)
  - void selectStyleOffset (int offset)
  - void selectPresetOffset (int offset)
  - bool handleCommonNav (int x, int y)

### Enums & Constants

- enum BrewWizardStep
  - `WIZARD_BATCH_NAME`
  - `WIZARD_BEER_STYLE`
  - `WIZARD_BATCH_SIZE`
  - `WIZARD_BRIX`
  - `WIZARD_AUTO_MODE`
  - `WIZARD_PLUG_CONTROL`
  - `WIZARD_YEAST`
  - `WIZARD_ATTENUATION`
  - `WIZARD_DIACETYL`
  - `WIZARD_YEAST_BEHAVIOR`
  - `WIZARD_TEMP_ADVANCED`
  - `WIZARD_REVIEW`
  - `WIZARD_DONE`

### Macros

- `#define BREW_WIZARD_CONTROLLER_H #include <Arduino.h>`

## Key Dependencies

**Local:**
- `SGNode_Base/src/ui/../domain/brew_profile.h`
- `SGNode_Base/src/ui/../domain/yeast_preset.h`

**System/External:**
- `<Arduino.h>`
- `<TFT_eSPI.h>`

## Side Effects & Hardware Access

- Touchscreen UI updates
- SPI bus communication
- Temperature target calculation

## Change Risks

- **MEDIUM**: UI changes affect user experience

---
*Last modified (hash): e6a99a71c857fba0a83d92e6623486c5f1bd6fabed9310bd792dc3acadfcc989*