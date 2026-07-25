# SGNode_Base/src/ui/brew_wizard_controller.cpp

**Type:** C++ Source
**Hash:** `e385792235a90f6444bce0b7f424fc4e461d06f3123e1e0e1badbd54b0297a92`

## Overview / Role

Control logic for temperature or other systems

## Verified API & Symbols

### Functions

- `void hitRect (int px, int py, int x, int y, int w, int h)`

- `void clampWizardFloat (float value, float minimum, float maximum)`

- `void appendChar (char* buffer, size_t bufferSize, char c, bool* clearOnInput)`

- `void draw (TFT_eSPI& tft, int step, int total, int x, int y, int w)`

- `void valueAt (int index)`

- `void count (void)`

- `void valueAt (int index)`

- `void count (void)`

- `void draw (TFT_eSPI& tft, int x, int y, bool enabled)`

- `void hit (int x, int y, int switchX, int switchY)`

- `void drawKey (TFT_eSPI& tft, int x, int y, int w, int h, const char* label) const`

- `void drawText (TFT_eSPI& tft, const char* value) const`

- `void drawNumber (TFT_eSPI& tft, const char* value) const`

- `void handleTextTouch (int x, int y, char* buffer, size_t bufferSize, bool* clearOnInput)`

- `void handleNumberTouch (int x, int y, char* buffer, size_t bufferSize, bool* clearOnInput)`

- `void BrewWizardController (void)`

- `void begin (BrewProfile* targetProfile)`

- `void clearResultFlags (void)`

- `void loadStepBuffer (void)`

- `void commitStepBuffer (void)`

- `void validateCurrentStep (void)`

- `void drawValidationMessage (TFT_eSPI& tft)`

- `void drawCancelConfirmation (TFT_eSPI& tft)`

- `void nextStep (void)`

- `void previousStep (void)`

- `void drawFrame (TFT_eSPI& tft, const char* title) const`

- `void drawNav (TFT_eSPI& tft, bool showBack, bool showNext, const char* nextLabel) const`

- `void drawDropdownSelector (TFT_eSPI& tft, const char* title, const char* selected, const char* hint) const`

- `void drawAttenuationList (TFT_eSPI& tft)`

- `void drawYeastHistory (TFT_eSPI& tft)`

- `void applySelectedPreset (void)`

- `void drawPresetSummary (TFT_eSPI& tft)`

- `void brewProfileFieldLabel (uint8_t index)`

- `void syncBrewProfileDerivedFields (void)`

- `void drawTemperatureAdvanced (TFT_eSPI& tft)`

- `void handleTemperatureAdvancedTouch (int x, int y)`

- `void selectStyleOffset (int offset)`

- `void selectPresetOffset (int offset)`

- `void draw (TFT_eSPI& tft)`

- `void handleCommonNav (int x, int y)`

- `void handleTouch (int x, int y)`

### Macros

- `#define WIZ_INPUT_Y 88`

- `#define WIZ_INPUT_H 36`

- `#define WIZ_TEXT_KEY_Y 130`

- `#define WIZ_KEY_H 28`

- `#define WIZ_NUM_KEY_Y 130`

- `#define WIZ_NAV_Y 276`

- `#define WIZ_DROPDOWN_Y 112`

- `#define WIZ_DROPDOWN_H 64`

- `#define WIZ_TEMP_ADV_Y 88`

- `#define WIZ_TEMP_ADV_ROW_H 28`

## Key Dependencies

**Local:**
- `SGNode_Base/src/ui/brew_wizard_controller.h`
- `SGNode_Base/src/ui/../calculations/brix_converter.h`
- `SGNode_Base/src/ui/../calculations/derived_calculations.h`
- `SGNode_Base/src/ui/../domain/yeast_preset_repository.h`
- `SGNode_Base/src/ui/../domain/batch_action.h`
- `SGNode_Base/src/ui/ui_tokens.h`
- `SGNode_Base/src/ui/ui_components.h`

**System/External:**
- `<string.h>`
- `<stdlib.h>`

## Side Effects & Hardware Access

- SD card read
- SD card file operations
- Touchscreen UI updates
- SPI bus communication
- Temperature target calculation

## Change Risks

- **MEDIUM**: UI changes affect user experience

---
*Last modified (hash): e385792235a90f6444bce0b7f424fc4e461d06f3123e1e0e1badbd54b0297a92*