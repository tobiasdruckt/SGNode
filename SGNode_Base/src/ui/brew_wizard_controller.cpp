#include "brew_wizard_controller.h"
#include "../calculations/brix_converter.h"
#include "../calculations/derived_calculations.h"
#include "../domain/yeast_preset_repository.h"
#include "../domain/batch_action.h"
#include "ui_tokens.h"
#include "ui_components.h"
#include <string.h>
#include <stdlib.h>

extern uint16_t uiColorPrimaryText;
extern uint16_t uiColorBackground;
extern uint16_t uiColorCardBackground;
extern uint16_t uiColorTextPrimary;
extern uint16_t uiColorTextSecondary;
extern uint16_t uiColorTextMuted;
extern uint16_t uiColorBorder;
extern uint16_t uiColorSuccess;
extern uint16_t uiColorGold;

static const char* STYLE_VALUES[] = {
  "German Pils", "Hoppy Pils", "Helles", "Koelsch",
  "Wheat Beer", "IPA", "Pale Ale", "Stout", "Porter",
  "Saison", "Lager", "Cider", "Mead", "Other"
};

static const int ATTENUATION_VALUES[] = {65, 68, 70, 72, 74, 75, 76, 78, 80, 82, 84, 86, 88};

#define WIZ_INPUT_Y 88
#define WIZ_INPUT_H 36
#define WIZ_TEXT_KEY_Y 130
#define WIZ_KEY_H 28
#define WIZ_NUM_KEY_Y 130
#define WIZ_NAV_Y 276
#define WIZ_DROPDOWN_Y 112
#define WIZ_DROPDOWN_H 64
#define WIZ_TEMP_ADV_Y 88
#define WIZ_TEMP_ADV_ROW_H 28

enum BrewProfileEditorField : uint8_t {
  BREW_FIELD_PITCH_TEMP,
  BREW_FIELD_MAIN_TEMP,
  BREW_FIELD_MAIN_HOLD,
  BREW_FIELD_NORMAL_RAMP,
  BREW_FIELD_DREST_TEMP,
  BREW_FIELD_DREST_HOLD,
  BREW_FIELD_DRY_HOP_OFFSET,
  BREW_FIELD_DRY_HOP_CONTACT,
  BREW_FIELD_CRASH_TEMP,
  BREW_FIELD_CRASH_HOLD,
  BREW_FIELD_CRASH_RAMP,
  BREW_FIELD_CARB_TEMP,
  BREW_FIELD_CARB_DAYS,
  BREW_FIELD_TARGET_CO2,
  BREW_FIELD_STORAGE_TEMP,
  BREW_FIELD_STORAGE_DAYS,
  BREW_PROFILE_FIELD_COUNT
};

static bool hitRect(int px, int py, int x, int y, int w, int h) {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

static float clampWizardFloat(float value, float minimum, float maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

static size_t wizardTextCursor = 0;

static void appendChar(char* buffer, size_t bufferSize, char c, bool* clearOnInput) {
  if (clearOnInput && *clearOnInput) {
    buffer[0] = '\0';
    *clearOnInput = false;
    wizardTextCursor = 0;
  }
  size_t l = strlen(buffer);
  if (wizardTextCursor > l) wizardTextCursor = l;
  if (l + 1 < bufferSize) {
    memmove(buffer + wizardTextCursor + 1, buffer + wizardTextCursor, l - wizardTextCursor + 1);
    buffer[wizardTextCursor] = c;
    wizardTextCursor++;
  }
}

void WizardStepper::draw(TFT_eSPI& tft, int step, int total, int x, int y, int w) {
  int gap = 5;
  int dotW = (w - gap * (total - 1)) / total;
  for (int i = 0; i < total; i++) {
    uint16_t color = i <= step ? uiColorGold : uiColorBorder;
    tft.fillRoundRect(x + i * (dotW + gap), y, dotW, 6, 3, color);
  }
}

const char* StyleDropdown::valueAt(int index) {
  if (index < 0 || index >= count()) return STYLE_VALUES[0];
  return STYLE_VALUES[index];
}

int StyleDropdown::count() {
  return sizeof(STYLE_VALUES) / sizeof(STYLE_VALUES[0]);
}

int AttenuationDropdown::valueAt(int index) {
  if (index < 0 || index >= count()) return ATTENUATION_VALUES[5];
  return ATTENUATION_VALUES[index];
}

int AttenuationDropdown::count() {
  return sizeof(ATTENUATION_VALUES) / sizeof(ATTENUATION_VALUES[0]);
}

void IOSSwitch::draw(TFT_eSPI& tft, int x, int y, bool enabled) {
  uint16_t bg = enabled ? uiColorSuccess : uiColorBorder;
  tft.fillRoundRect(x, y, 60, 32, 16, bg);
  tft.fillCircle(enabled ? x + 44 : x + 16, y + 16, 13, TFT_WHITE);
}

bool IOSSwitch::hit(int x, int y, int switchX, int switchY) {
  return hitRect(x, y, switchX, switchY, 60, 32);
}

static void drawKey(TFT_eSPI& tft, int x, int y, int w, int h, const char* label) {
  tft.fillRoundRect(x, y, w, h, 6, uiColorCardBackground);
  tft.drawRoundRect(x, y, w, h, 6, uiColorBorder);
  uiTextCenter(x, y, w, h, label, FONT_SIZE_SM, uiColorTextPrimary);
}

void VirtualKeyboardInput::drawText(TFT_eSPI& tft, const char* value) {
  uiCard(MARGIN, WIZ_INPUT_Y, UI_W - MARGIN * 2, WIZ_INPUT_H, 8);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM_BOLD);
  tft.setCursor(MARGIN + 12, WIZ_INPUT_Y + 26);
  tft.print(value);
  size_t valueLen = strlen(value);
  if (wizardTextCursor > valueLen) wizardTextCursor = valueLen;
  char beforeCursor[40];
  size_t cursorLen = wizardTextCursor;
  if (cursorLen >= sizeof(beforeCursor)) cursorLen = sizeof(beforeCursor) - 1;
  memcpy(beforeCursor, value, cursorLen);
  beforeCursor[cursorLen] = '\0';
  int cursorX = MARGIN + 12 + tft.textWidth(beforeCursor);
  tft.drawFastVLine(cursorX, WIZ_INPUT_Y + 8, WIZ_INPUT_H - 14, uiColorGold);

  const char* rows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  int y = WIZ_TEXT_KEY_Y;
  for (int r = 0; r < 3; r++) {
    int len = strlen(rows[r]);
    int keyW = r == 0 ? 40 : 44;
    int x = MARGIN + (r * 18);
    for (int i = 0; i < len; i++) {
      char key[2] = {rows[r][i], '\0'};
      drawKey(tft, x + i * (keyW + 3), y, keyW, WIZ_KEY_H, key);
    }
    y += 34;
  }
  drawKey(tft, MARGIN, 232, 52, WIZ_KEY_H, "<");
  drawKey(tft, MARGIN + 60, 232, 120, WIZ_KEY_H, "SPACE");
  drawKey(tft, MARGIN + 188, 232, 52, WIZ_KEY_H, ">");
  drawKey(tft, MARGIN + 248, 232, 94, WIZ_KEY_H, "DEL");
}

void VirtualKeyboardInput::drawNumber(TFT_eSPI& tft, const char* value) {
  uiCard(MARGIN, WIZ_INPUT_Y, UI_W - MARGIN * 2, WIZ_INPUT_H, 8);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM_BOLD);
  tft.setCursor(MARGIN + 12, WIZ_INPUT_Y + 26);
  tft.print(value);

  const char* keys[] = {"1","2","3","4","5","6","7","8","9",".","0","DEL"};
  int keyW = 94;
  int keyH = 32;
  int startX = 92;
  int startY = WIZ_NUM_KEY_Y;
  for (int i = 0; i < 12; i++) {
    int col = i % 3;
    int row = i / 3;
    drawKey(tft, startX + col * (keyW + 8), startY + row * (keyH + 5), keyW, keyH, keys[i]);
  }
}

bool VirtualKeyboardInput::handleTextTouch(int x, int y, char* buffer, size_t bufferSize, bool* clearOnInput) {
  const char* rows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  int keyY = WIZ_TEXT_KEY_Y;
  for (int r = 0; r < 3; r++) {
    int len = strlen(rows[r]);
    int keyW = r == 0 ? 40 : 44;
    int keyX = MARGIN + (r * 18);
    for (int i = 0; i < len; i++) {
      if (hitRect(x, y, keyX + i * (keyW + 3), keyY, keyW, WIZ_KEY_H)) {
        appendChar(buffer, bufferSize, rows[r][i], clearOnInput);
        return true;
      }
    }
    keyY += 34;
  }
  if (hitRect(x, y, MARGIN, 232, 52, WIZ_KEY_H)) {
    if (wizardTextCursor > 0) wizardTextCursor--;
    return true;
  }
  if (hitRect(x, y, MARGIN + 60, 232, 120, WIZ_KEY_H)) {
    appendChar(buffer, bufferSize, ' ', clearOnInput);
    return true;
  }
  if (hitRect(x, y, MARGIN + 188, 232, 52, WIZ_KEY_H)) {
    size_t l = strlen(buffer);
    if (wizardTextCursor < l) wizardTextCursor++;
    return true;
  }
  if (hitRect(x, y, MARGIN + 248, 232, 94, WIZ_KEY_H)) {
    if (clearOnInput && *clearOnInput) {
      buffer[0] = '\0';
      *clearOnInput = false;
      wizardTextCursor = 0;
      return true;
    }
    size_t l = strlen(buffer);
    if (wizardTextCursor > l) wizardTextCursor = l;
    if (wizardTextCursor > 0 && l > 0) {
      memmove(buffer + wizardTextCursor - 1, buffer + wizardTextCursor, l - wizardTextCursor + 1);
      wizardTextCursor--;
    }
    return true;
  }
  return false;
}

bool VirtualKeyboardInput::handleNumberTouch(int x, int y, char* buffer, size_t bufferSize, bool* clearOnInput) {
  const char* keys[] = {"1","2","3","4","5","6","7","8","9",".","0","DEL"};
  int keyW = 94;
  int keyH = 32;
  int startX = 92;
  int startY = WIZ_NUM_KEY_Y;
  for (int i = 0; i < 12; i++) {
    int col = i % 3;
    int row = i / 3;
    if (!hitRect(x, y, startX + col * (keyW + 8), startY + row * (keyH + 5), keyW, keyH)) continue;
    if (strcmp(keys[i], "DEL") == 0) {
      if (clearOnInput && *clearOnInput) {
        buffer[0] = '\0';
        *clearOnInput = false;
        return true;
      }
      size_t l = strlen(buffer);
      if (l > 0) buffer[l - 1] = '\0';
    } else {
      appendChar(buffer, bufferSize, keys[i][0], clearOnInput);
    }
    return true;
  }
  return false;
}

BrewWizardController::BrewWizardController() {
  profile = NULL;
  step = WIZARD_BATCH_NAME;
  isComplete = false;
  isCancelled = false;
  confirmCancel = false;
  editBuffer[0] = '\0';
  keyboardUpper = true;
  editPristine = false;
  yeastHistoryCount = 0;
  brewProfileSelectedIndex = 0;
  brewProfileFirstVisibleIndex = 0;
}

void BrewWizardController::begin(BrewProfile* targetProfile) {
  profile = targetProfile;
  step = WIZARD_BATCH_NAME;
  isComplete = false;
  isCancelled = false;
  confirmCancel = false;
  validationMessage[0] = '\0';
  yeastHistoryCount = BrewProfileStore::loadYeastHistory(yeastHistory, 3);
  if (profile && profile->autoModeEnabled) applySelectedPreset();
  brewProfileSelectedIndex = 0;
  brewProfileFirstVisibleIndex = 0;
  loadStepBuffer();
}

bool BrewWizardController::completed() const { return isComplete; }
bool BrewWizardController::cancelled() const { return isCancelled; }
bool BrewWizardController::cancelConfirmationVisible() const { return confirmCancel; }
void BrewWizardController::clearResultFlags() { isComplete = false; isCancelled = false; confirmCancel = false; }
BrewWizardStep BrewWizardController::currentStep() const { return step; }

void BrewWizardController::loadStepBuffer() {
  if (!profile) return;
  editPristine = false;
  switch (step) {
    case WIZARD_BATCH_NAME:
      strncpy(editBuffer, profile->batchName, sizeof(editBuffer));
      editPristine = true;
      wizardTextCursor = strlen(editBuffer);
      break;
    case WIZARD_BATCH_SIZE:
      snprintf(editBuffer, sizeof(editBuffer), "%.1f", profile->batchSizeLiters);
      editPristine = true;
      break;
    case WIZARD_BRIX:
      snprintf(editBuffer, sizeof(editBuffer), "%.1f", profile->recipeBrix);
      editPristine = true;
      break;
    case WIZARD_YEAST:
      strncpy(editBuffer, profile->yeastName, sizeof(editBuffer));
      editPristine = true;
      wizardTextCursor = strlen(editBuffer);
      break;
    default:
      editBuffer[0] = '\0';
      break;
  }
  editBuffer[sizeof(editBuffer) - 1] = '\0';
}

void BrewWizardController::commitStepBuffer() {
  if (!profile) return;
  switch (step) {
    case WIZARD_BATCH_NAME:
      if (editBuffer[0]) strncpy(profile->batchName, editBuffer, sizeof(profile->batchName) - 1);
      profile->batchName[sizeof(profile->batchName) - 1] = '\0';
      break;
    case WIZARD_BATCH_SIZE:
      profile->batchSizeLiters = atof(editBuffer);
      break;
    case WIZARD_BRIX:
      profile->recipeBrix = atof(editBuffer);
      profile->recipeOG = BrixConverter::brixToSG(profile->recipeBrix);
      profile->effectiveOG = profile->recipeOG;
      profile->expectedFinalGravity = DerivedCalculations::expectedFG(profile->effectiveOG, profile->expectedApparentAttenuation);
      break;
    case WIZARD_YEAST:
      if (!profile->autoModeEnabled) {
        if (editBuffer[0]) strncpy(profile->yeastName, editBuffer, sizeof(profile->yeastName) - 1);
        profile->yeastName[sizeof(profile->yeastName) - 1] = '\0';
        strcpy(profile->attenuationSource, "manual");
      }
      break;
    default:
      break;
  }
}

bool BrewWizardController::validateCurrentStep() {
  validationMessage[0] = '\0';
  if (!profile) return false;

  if (step == WIZARD_BATCH_SIZE) {
    float liters = atof(editBuffer);
    if (liters < 1.0f || liters > 200.0f) {
      strncpy(validationMessage, "Use 1.0-200.0 L", sizeof(validationMessage) - 1);
      validationMessage[sizeof(validationMessage) - 1] = '\0';
      return false;
    }
  } else if (step == WIZARD_BRIX) {
    float brix = atof(editBuffer);
    if (brix < 2.0f || brix > 35.0f) {
      strncpy(validationMessage, "Use 2.0-35.0 Brix", sizeof(validationMessage) - 1);
      validationMessage[sizeof(validationMessage) - 1] = '\0';
      return false;
    }
  }

  return true;
}

void BrewWizardController::drawValidationMessage(TFT_eSPI& tft) {
  if (validationMessage[0] == '\0') return;
  tft.setTextColor(TFT_RED);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN, WIZ_INPUT_Y + 54);
  tft.print(validationMessage);
}

void BrewWizardController::drawCancelConfirmation(TFT_eSPI& tft) {
  if (!confirmCancel) return;
  int x = MARGIN + 28;
  int y = 86;
  int w = UI_W - (MARGIN + 28) * 2;
  int h = 132;
  tft.fillRoundRect(x, y, w, h, 8, uiColorCardBackground);
  tft.drawRoundRect(x, y, w, h, 8, uiColorBorder);
  uiTextCenter(x, y + 18, w, 30, "Discard changes?", FONT_SIZE_MD, uiColorTextPrimary);
  tft.drawRoundRect(x + 14, y + 68, 116, 38, 8, uiColorBorder);
  uiTextCenter(x + 14, y + 68, 116, 38, "NO", FONT_SIZE_SM, uiColorTextPrimary);
  tft.fillRoundRect(x + w - 130, y + 68, 116, 38, 8, uiColorGold);
  uiTextCenter(x + w - 130, y + 68, 116, 38, "YES", FONT_SIZE_SM, uiColorPrimaryText);
}

void BrewWizardController::nextStep() {
  if (!validateCurrentStep()) return;
  commitStepBuffer();
  if (step == WIZARD_TEMP_ADVANCED) {
    syncBrewProfileDerivedFields();
  }
  if (step == WIZARD_REVIEW) {
    isComplete = true;
    step = WIZARD_DONE;
    return;
  }
  if (profile && step == WIZARD_AUTO_MODE) {
    step = WIZARD_PLUG_CONTROL;
  } else if (profile && profile->autoModeEnabled && step == WIZARD_YEAST) {
    step = WIZARD_YEAST_BEHAVIOR;
  } else if (profile && profile->autoModeEnabled && step == WIZARD_YEAST_BEHAVIOR) {
    step = WIZARD_TEMP_ADVANCED;
  } else if (profile && !profile->autoModeEnabled && step == WIZARD_DIACETYL) {
    step = WIZARD_TEMP_ADVANCED;
  } else if (step == WIZARD_TEMP_ADVANCED) {
    step = WIZARD_REVIEW;
  } else {
    step = (BrewWizardStep)((int)step + 1);
  }
  if (step == WIZARD_TEMP_ADVANCED && profile && !profile->temperatureProfile.enabled) {
    TemperatureProfileEngine::generateForProfile(profile);
  }
  if (step == WIZARD_TEMP_ADVANCED) {
    syncBrewProfileDerivedFields();
    brewProfileSelectedIndex = 0;
    brewProfileFirstVisibleIndex = 0;
  }
  loadStepBuffer();
}

void BrewWizardController::previousStep() {
  if (step == WIZARD_BATCH_NAME) {
    confirmCancel = true;
    return;
  }
  commitStepBuffer();
  if (profile && step == WIZARD_PLUG_CONTROL) {
    step = WIZARD_AUTO_MODE;
  } else if (step == WIZARD_REVIEW) {
    step = WIZARD_TEMP_ADVANCED;
  } else if (profile && profile->autoModeEnabled && step == WIZARD_TEMP_ADVANCED) {
    step = WIZARD_YEAST_BEHAVIOR;
  } else if (profile && !profile->autoModeEnabled && step == WIZARD_TEMP_ADVANCED) {
    step = WIZARD_DIACETYL;
  } else if (profile && profile->autoModeEnabled && step == WIZARD_YEAST) {
    step = WIZARD_PLUG_CONTROL;
  } else if (profile && profile->autoModeEnabled && step == WIZARD_YEAST_BEHAVIOR) {
    step = WIZARD_YEAST;
  } else {
    step = (BrewWizardStep)((int)step - 1);
  }
  loadStepBuffer();
}

void BrewWizardController::drawFrame(TFT_eSPI& tft, const char* title) {
  tft.fillScreen(uiColorBackground);
  uiDrawTopbar("Brew Wizard", true, true, 0);
  WizardStepper::draw(tft, (int)step, 12, MARGIN, TOPBAR_H + 10, UI_W - MARGIN * 2);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM_BOLD);
  tft.setCursor(MARGIN, TOPBAR_H + 42);
  tft.print(title);
}

void BrewWizardController::drawNav(TFT_eSPI& tft, bool showBack, bool showNext, const char* nextLabel) {
  if (showBack) {
    tft.fillRoundRect(MARGIN, WIZ_NAV_Y, 120, 36, 8, uiColorCardBackground);
    tft.drawRoundRect(MARGIN, WIZ_NAV_Y, 120, 36, 8, uiColorBorder);
    uiTextCenter(MARGIN, WIZ_NAV_Y, 120, 36, "BACK", FONT_SIZE_SM, uiColorTextPrimary);
  }
  if (showNext) {
    tft.fillRoundRect(UI_W - MARGIN - 140, WIZ_NAV_Y, 140, 36, 8, uiColorGold);
    uiTextCenter(UI_W - MARGIN - 140, WIZ_NAV_Y, 140, 36, nextLabel, FONT_SIZE_SM, uiColorPrimaryText);
  }
}

void BrewWizardController::drawDropdownSelector(TFT_eSPI& tft, const char* title, const char* selected, const char* hint) {
  if (title && title[0]) {
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN, 94);
    tft.print(title);
  }
  tft.fillRoundRect(MARGIN, WIZ_DROPDOWN_Y, 58, WIZ_DROPDOWN_H, 8, uiColorCardBackground);
  tft.drawRoundRect(MARGIN, WIZ_DROPDOWN_Y, 58, WIZ_DROPDOWN_H, 8, uiColorBorder);
  uiTextCenter(MARGIN, WIZ_DROPDOWN_Y, 58, WIZ_DROPDOWN_H, "<", FONT_SIZE_MD, uiColorTextPrimary);

  int centerX = MARGIN + 66;
  int centerW = UI_W - MARGIN * 2 - 132;
  tft.fillRoundRect(centerX, WIZ_DROPDOWN_Y, centerW, WIZ_DROPDOWN_H, 8, uiColorCardBackground);
  tft.drawRoundRect(centerX, WIZ_DROPDOWN_Y, centerW, WIZ_DROPDOWN_H, 8, uiColorBorder);
  uiTextCenter(centerX, WIZ_DROPDOWN_Y, centerW, WIZ_DROPDOWN_H, selected, FONT_SIZE_SM_BOLD, uiColorTextPrimary);

  tft.fillRoundRect(UI_W - MARGIN - 58, WIZ_DROPDOWN_Y, 58, WIZ_DROPDOWN_H, 8, uiColorCardBackground);
  tft.drawRoundRect(UI_W - MARGIN - 58, WIZ_DROPDOWN_Y, 58, WIZ_DROPDOWN_H, 8, uiColorBorder);
  uiTextCenter(UI_W - MARGIN - 58, WIZ_DROPDOWN_Y, 58, WIZ_DROPDOWN_H, ">", FONT_SIZE_MD, uiColorTextPrimary);

  if (hint && hint[0]) {
    tft.setTextColor(uiColorTextMuted);
    tft.setFreeFont(FONT_SIZE_XS);
    tft.setCursor(MARGIN, WIZ_DROPDOWN_Y + WIZ_DROPDOWN_H + 24);
    tft.print(hint);
  }
}

void BrewWizardController::drawAttenuationList(TFT_eSPI& tft) {
  int x = MARGIN;
  int y = 88;
  int w = 66;
  int h = 32;
  for (int i = 0; i < AttenuationDropdown::count(); i++) {
    int col = i % 6;
    int row = i / 6;
    int bx = x + col * (w + 9);
    int by = y + row * (h + 10);
    char label[8];
    snprintf(label, sizeof(label), "%d", AttenuationDropdown::valueAt(i));
    bool active = profile && profile->expectedApparentAttenuation == AttenuationDropdown::valueAt(i);
    tft.fillRoundRect(bx, by, w, h, 6, active ? uiColorGold : uiColorCardBackground);
    tft.drawRoundRect(bx, by, w, h, 6, uiColorBorder);
    uiTextCenter(bx, by, w, h, label, FONT_SIZE_SM, active ? uiColorPrimaryText : uiColorTextPrimary);
  }
}

void BrewWizardController::drawYeastHistory(TFT_eSPI& tft) {
  if (yeastHistoryCount <= 0) return;
  int x = MARGIN + 210;
  int y = 232;
  int w = 80;
  for (int i = 0; i < yeastHistoryCount; i++) {
    int bx = x + i * (w + 6);
    tft.fillRoundRect(bx, y, w, WIZ_KEY_H, 6, uiColorCardBackground);
    tft.drawRoundRect(bx, y, w, WIZ_KEY_H, 6, uiColorBorder);
    char label[16];
    strncpy(label, yeastHistory[i], sizeof(label) - 1);
    label[sizeof(label) - 1] = '\0';
    uiTextCenter(bx, y, w, WIZ_KEY_H, label, FONT_SIZE_XS, uiColorTextPrimary);
  }
}

void BrewWizardController::applySelectedPreset() {
  if (!profile) return;
  const YeastPreset* preset = YeastPresetRepository::findById(profile->selectedYeastPresetId);
  YeastPresetRepository::applyToProfile(*preset, profile);
  syncBrewProfileDerivedFields();
}

void BrewWizardController::drawPresetSummary(TFT_eSPI& tft) {
  if (!profile) return;
  uiCard(MARGIN, 92, UI_W - MARGIN * 2, 148, 8);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM_BOLD);
  tft.setCursor(MARGIN + 12, 116);
  tft.print(profile->selectedYeastPresetName);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN + 12, 142);
  tft.printf("Atten: %d-%d%%, default %d%%", profile->yeastAttenuationMin, profile->yeastAttenuationMax, profile->yeastDefaultAttenuation);
  tft.setCursor(MARGIN + 12, 168);
  tft.printf("Temp: %.0f-%.0f C, %s", profile->recommendedTempMinC, profile->recommendedTempMaxC, profile->fermentationSpeed);
  tft.setCursor(MARGIN + 12, 194);
  tft.printf("Profile: %.0f C pitch, %.0f C main",
             profile->temperatureProfile.advanced.pitchC,
             profile->temperatureProfile.advanced.mainC);
  tft.setCursor(MARGIN + 12, 220);
  if (profile->temperatureProfile.advanced.dRestHoldHours > 0) {
    tft.printf("D-rest %.0f C, crash %.0f C",
               profile->temperatureProfile.advanced.dRestC,
               profile->temperatureProfile.advanced.crashC);
  } else {
    tft.printf("Crash %.0f C, package %.0f C",
               profile->temperatureProfile.advanced.crashC,
               profile->temperatureProfile.advanced.carbonationC);
  }
}

static const char* brewProfileFieldLabel(uint8_t index) {
  switch (index) {
    case BREW_FIELD_PITCH_TEMP: return "Pitch Temperature";
    case BREW_FIELD_MAIN_TEMP: return "Main Temperature";
    case BREW_FIELD_MAIN_HOLD: return "Main Hold Time";
    case BREW_FIELD_NORMAL_RAMP: return "Normal Ramp";
    case BREW_FIELD_DREST_TEMP: return "Diacetyl Rest Temperature";
    case BREW_FIELD_DREST_HOLD: return "Diacetyl Rest Hold Time";
    case BREW_FIELD_DRY_HOP_OFFSET: return "Dry Hop SG Offset";
    case BREW_FIELD_DRY_HOP_CONTACT: return "Dry Hop Contact Time";
    case BREW_FIELD_CRASH_TEMP: return "Cold Crash Temperature";
    case BREW_FIELD_CRASH_HOLD: return "Cold Crash Hold Time";
    case BREW_FIELD_CRASH_RAMP: return "Cold Crash Ramp";
    case BREW_FIELD_CARB_TEMP: return "Carbonation Temperature";
    case BREW_FIELD_CARB_DAYS: return "Carbonation Days";
    case BREW_FIELD_TARGET_CO2: return "Target CO2";
    case BREW_FIELD_STORAGE_TEMP: return "Storage Temperature";
    case BREW_FIELD_STORAGE_DAYS: return "Storage Days Hint";
    default: return "";
  }
}

void BrewWizardController::syncBrewProfileDerivedFields() {
  if (!profile) return;
  TemperatureProfileAdvanced& adv = profile->temperatureProfile.advanced;
  profile->temperatureProfile.enabled = true;
  profile->diacetylRestEnabled = adv.dRestHoldHours > 0;
  profile->dryHopEnabled = profile->dryHopFgOffset > 0.0f && profile->dryHopContactHours > 0;
  if (profile->expectedFinalGravity > 1.0f) {
    profile->dryHopTriggerSG = profile->expectedFinalGravity + profile->dryHopFgOffset;
  }
  profile->estimatedABV = DerivedCalculations::abv(profile->effectiveOG, profile->expectedFinalGravity);
  TemperatureProfileEngine::rebuildPhaseList(&profile->temperatureProfile);
}

void BrewWizardController::drawTemperatureAdvanced(TFT_eSPI& tft) {
  if (!profile) return;
  const uint8_t visibleRows = 6;
  const int listX = MARGIN;
  const int listY = 82;
  const int listW = UI_W - MARGIN * 2;
  const int rowH = 30;

  if (brewProfileSelectedIndex >= BREW_PROFILE_FIELD_COUNT) brewProfileSelectedIndex = 0;
  if (brewProfileSelectedIndex < brewProfileFirstVisibleIndex) {
    brewProfileFirstVisibleIndex = brewProfileSelectedIndex;
  }
  if (brewProfileSelectedIndex >= brewProfileFirstVisibleIndex + visibleRows) {
    brewProfileFirstVisibleIndex = brewProfileSelectedIndex - visibleRows + 1;
  }
  if (brewProfileFirstVisibleIndex + visibleRows > BREW_PROFILE_FIELD_COUNT) {
    brewProfileFirstVisibleIndex = BREW_PROFILE_FIELD_COUNT > visibleRows
      ? BREW_PROFILE_FIELD_COUNT - visibleRows
      : 0;
  }

  char value[20];
  const TemperatureProfileAdvanced& adv = profile->temperatureProfile.advanced;
  for (uint8_t row = 0; row < visibleRows; row++) {
    uint8_t index = brewProfileFirstVisibleIndex + row;
    if (index >= BREW_PROFILE_FIELD_COUNT) break;
    int y = listY + row * rowH;
    bool selected = index == brewProfileSelectedIndex;
    if (selected) {
      tft.fillRoundRect(listX, y - 2, listW, rowH - 2, 6, uiColorGold);
    } else {
      tft.fillRoundRect(listX, y - 2, listW, rowH - 2, 6, uiColorCardBackground);
      tft.drawRoundRect(listX, y - 2, listW, rowH - 2, 6, uiColorBorder);
    }
    switch (index) {
      case BREW_FIELD_PITCH_TEMP: snprintf(value, sizeof(value), "%.1f C", adv.pitchC); break;
      case BREW_FIELD_MAIN_TEMP: snprintf(value, sizeof(value), "%.1f C", adv.mainC); break;
      case BREW_FIELD_MAIN_HOLD: snprintf(value, sizeof(value), "%lu h", adv.mainHoldHours); break;
      case BREW_FIELD_DREST_TEMP: snprintf(value, sizeof(value), "%.1f C", adv.dRestC); break;
      case BREW_FIELD_DREST_HOLD: snprintf(value, sizeof(value), adv.dRestHoldHours == 0 ? "N/A" : "%lu h", adv.dRestHoldHours); break;
      case BREW_FIELD_CRASH_TEMP: snprintf(value, sizeof(value), "%.1f C", adv.crashC); break;
      case BREW_FIELD_CRASH_HOLD: snprintf(value, sizeof(value), adv.crashHoldHours == 0 ? "N/A" : "%lu h", adv.crashHoldHours); break;
      case BREW_FIELD_NORMAL_RAMP: snprintf(value, sizeof(value), "%.1f K/h", adv.normalRampKPerH); break;
      case BREW_FIELD_CRASH_RAMP: snprintf(value, sizeof(value), "%.1f K/h", adv.coldCrashRampKPerH); break;
      case BREW_FIELD_CARB_TEMP: snprintf(value, sizeof(value), "%.1f C", adv.carbonationC); break;
      case BREW_FIELD_CARB_DAYS: snprintf(value, sizeof(value), "%lu d", adv.carbonationDays); break;
      case BREW_FIELD_TARGET_CO2: snprintf(value, sizeof(value), "%.1f vol", adv.targetCO2); break;
      case BREW_FIELD_STORAGE_TEMP: snprintf(value, sizeof(value), "%.1f C", adv.storageC); break;
      case BREW_FIELD_STORAGE_DAYS: snprintf(value, sizeof(value), "%lu d", adv.storageDaysHint); break;
      case BREW_FIELD_DRY_HOP_OFFSET: snprintf(value, sizeof(value), profile->dryHopFgOffset <= 0.0f ? "N/A" : "+%.3f SG", profile->dryHopFgOffset); break;
      case BREW_FIELD_DRY_HOP_CONTACT: snprintf(value, sizeof(value), profile->dryHopContactHours == 0 ? "N/A" : "%lu h", profile->dryHopContactHours); break;
      default: value[0] = '\0'; break;
    }
    tft.setFreeFont(selected ? FONT_SIZE_SM_BOLD : FONT_SIZE_SM);
    tft.setTextColor(selected ? uiColorPrimaryText : uiColorTextSecondary);
    tft.setCursor(listX + 10, y + 18);
    tft.print(brewProfileFieldLabel(index));
    uiTextRight(listX + 275, y + 1, listW - 288, 20, value,
                FONT_SIZE_SM, selected ? uiColorPrimaryText : uiColorTextPrimary);
  }

  const int ctrlY = WIZ_NAV_Y;
  const int ctrlW = 38;
  const int ctrlX0 = MARGIN + 136;
  const char* labels[] = {"UP", "DN", "-", "+"};
  for (int i = 0; i < 4; i++) {
    int bx = ctrlX0 + i * 42;
    tft.fillRoundRect(bx, ctrlY, ctrlW, 36, 8, uiColorCardBackground);
    tft.drawRoundRect(bx, ctrlY, ctrlW, 36, 8, uiColorBorder);
    uiTextCenter(bx, ctrlY, ctrlW, 36, labels[i], FONT_SIZE_SM, uiColorTextPrimary);
  }
}

bool BrewWizardController::handleTemperatureAdvancedTouch(int x, int y) {
  if (!profile) return false;
  const uint8_t visibleRows = 6;
  const int listY = 82;
  const int rowH = 30;
  if (y >= listY && y < listY + visibleRows * rowH) {
    uint8_t row = (y - listY) / rowH;
    uint8_t index = brewProfileFirstVisibleIndex + row;
    if (index < BREW_PROFILE_FIELD_COUNT) {
      brewProfileSelectedIndex = index;
      return true;
    }
  }
  if (y < WIZ_NAV_Y || y > WIZ_NAV_Y + 36) return false;
  const int ctrlW = 38;
  const int ctrlX0 = MARGIN + 136;
  if (hitRect(x, y, ctrlX0, WIZ_NAV_Y, ctrlW, 36)) {
    if (brewProfileSelectedIndex > 0) brewProfileSelectedIndex--;
    return true;
  }
  if (hitRect(x, y, ctrlX0 + 42, WIZ_NAV_Y, ctrlW, 36)) {
    if (brewProfileSelectedIndex + 1 < BREW_PROFILE_FIELD_COUNT) brewProfileSelectedIndex++;
    return true;
  }
  bool minus = hitRect(x, y, ctrlX0 + 84, WIZ_NAV_Y, ctrlW, 36);
  bool plus = hitRect(x, y, ctrlX0 + 126, WIZ_NAV_Y, ctrlW, 36);
  if (!minus && !plus) return false;

  float dir = plus ? 1.0f : -1.0f;
  TemperatureProfileAdvanced& adv = profile->temperatureProfile.advanced;
  switch (brewProfileSelectedIndex) {
    case BREW_FIELD_PITCH_TEMP: adv.pitchC = clampWizardFloat(adv.pitchC + dir * 0.5f, -2.0f, 35.0f); break;
    case BREW_FIELD_MAIN_TEMP: adv.mainC = clampWizardFloat(adv.mainC + dir * 0.5f, -2.0f, 35.0f); break;
    case BREW_FIELD_MAIN_HOLD:
      if (plus && adv.mainHoldHours < 336) adv.mainHoldHours += 12;
      if (minus && adv.mainHoldHours > 12) adv.mainHoldHours -= 12;
      break;
    case BREW_FIELD_DREST_TEMP: adv.dRestC = clampWizardFloat(adv.dRestC + dir * 0.5f, 0.0f, 35.0f); break;
    case BREW_FIELD_DREST_HOLD:
      if (plus && adv.dRestHoldHours < 96) adv.dRestHoldHours += 12;
      if (minus && adv.dRestHoldHours >= 12) adv.dRestHoldHours -= 12;
      break;
    case BREW_FIELD_CRASH_TEMP: adv.crashC = clampWizardFloat(adv.crashC + dir * 0.5f, 0.0f, 12.0f); break;
    case BREW_FIELD_CRASH_HOLD:
      if (plus && adv.crashHoldHours < 96) adv.crashHoldHours += 12;
      if (minus && adv.crashHoldHours >= 12) adv.crashHoldHours -= 12;
      break;
    case BREW_FIELD_NORMAL_RAMP: adv.normalRampKPerH = clampWizardFloat(adv.normalRampKPerH + dir * 0.1f, 0.0f, 2.0f); break;
    case BREW_FIELD_CRASH_RAMP: adv.coldCrashRampKPerH = clampWizardFloat(adv.coldCrashRampKPerH + dir * 0.1f, 0.0f, 4.0f); break;
    case BREW_FIELD_CARB_TEMP: adv.carbonationC = clampWizardFloat(adv.carbonationC + dir * 0.5f, 10.0f, 25.0f); break;
    case BREW_FIELD_CARB_DAYS:
      if (plus && adv.carbonationDays < 30) adv.carbonationDays++;
      if (minus && adv.carbonationDays > 1) adv.carbonationDays--;
      break;
    case BREW_FIELD_TARGET_CO2: adv.targetCO2 = clampWizardFloat(adv.targetCO2 + dir * 0.1f, 1.5f, 4.0f); break;
    case BREW_FIELD_STORAGE_TEMP: adv.storageC = clampWizardFloat(adv.storageC + dir * 0.5f, 0.0f, 12.0f); break;
    case BREW_FIELD_STORAGE_DAYS:
      if (plus && adv.storageDaysHint < 180) adv.storageDaysHint++;
      if (minus && adv.storageDaysHint > 1) adv.storageDaysHint--;
      break;
    case BREW_FIELD_DRY_HOP_OFFSET:
      profile->dryHopFgOffset = clampWizardFloat(profile->dryHopFgOffset + dir * 0.001f, 0.0f, 0.020f);
      break;
    case BREW_FIELD_DRY_HOP_CONTACT:
      if (plus && profile->dryHopContactHours < 168) profile->dryHopContactHours += 12;
      if (minus && profile->dryHopContactHours >= 12) profile->dryHopContactHours -= 12;
      break;
  }
  syncBrewProfileDerivedFields();
  return true;
}

int BrewWizardController::currentStyleIndex() const {
  if (!profile) return 0;
  for (int i = 0; i < StyleDropdown::count(); i++) {
    if (strcmp(profile->beerStyle, StyleDropdown::valueAt(i)) == 0) return i;
  }
  return 0;
}

int BrewWizardController::currentPresetIndex() const {
  if (!profile) return 0;
  for (int i = 0; i < YeastPresetRepository::count(); i++) {
    if (strcmp(profile->selectedYeastPresetId, YeastPresetRepository::at(i)->id) == 0) return i;
  }
  return 0;
}

void BrewWizardController::selectStyleOffset(int offset) {
  if (!profile) return;
  int total = StyleDropdown::count();
  int idx = currentStyleIndex() + offset;
  if (idx < 0) idx = total - 1;
  if (idx >= total) idx = 0;
  strncpy(profile->beerStyle, StyleDropdown::valueAt(idx), sizeof(profile->beerStyle) - 1);
  profile->beerStyle[sizeof(profile->beerStyle) - 1] = '\0';
  BatchActionEngine::applyStyleDefaults(profile);
  TemperatureProfileEngine::generateForProfile(profile);
  syncBrewProfileDerivedFields();
}

void BrewWizardController::selectPresetOffset(int offset) {
  if (!profile) return;
  int total = YeastPresetRepository::count();
  int idx = currentPresetIndex() + offset;
  if (idx < 0) idx = total - 1;
  if (idx >= total) idx = 0;
  YeastPresetRepository::applyToProfile(*YeastPresetRepository::at(idx), profile);
}

void BrewWizardController::draw(TFT_eSPI& tft) {
  if (!profile) return;
  switch (step) {
    case WIZARD_BATCH_NAME:
      drawFrame(tft, "Batch Name");
      VirtualKeyboardInput::drawText(tft, editBuffer);
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_BEER_STYLE:
      drawFrame(tft, "Beer Style");
      drawDropdownSelector(tft, "", profile->beerStyle, "Use arrows to choose the beer style");
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_BATCH_SIZE:
      drawFrame(tft, "Batch Size (L)");
      VirtualKeyboardInput::drawNumber(tft, editBuffer);
      drawValidationMessage(tft);
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_BRIX:
      drawFrame(tft, "Original Sugar Brix");
      VirtualKeyboardInput::drawNumber(tft, editBuffer);
      tft.setTextColor(uiColorTextSecondary);
      tft.setFreeFont(FONT_SIZE_SM);
      tft.setCursor(UI_W - MARGIN - 125, WIZ_INPUT_Y + 26);
      tft.printf("OG %.3f", BrixConverter::brixToSG(atof(editBuffer)));
      drawValidationMessage(tft);
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_AUTO_MODE:
      drawFrame(tft, "Auto Mode");
      tft.setTextColor(uiColorTextSecondary);
      tft.setFreeFont(FONT_SIZE_SM);
      tft.setCursor(MARGIN, 110);
      tft.print("Use yeast behavior preset");
      IOSSwitch::draw(tft, UI_W - MARGIN - 74, 86, profile->autoModeEnabled);
      tft.setCursor(MARGIN, 166);
      tft.print(profile->autoModeEnabled ? "Preset fills attenuation, curve and ETA" : "Manual yeast and attenuation");
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_PLUG_CONTROL:
      drawFrame(tft, "SGNode Plug");
      tft.setTextColor(uiColorTextSecondary);
      tft.setFreeFont(FONT_SIZE_SM);
      tft.setCursor(MARGIN, 110);
      tft.print("Use Plug for temperature control");
      IOSSwitch::draw(tft, UI_W - MARGIN - 74, 86, profile->plugControlEnabled);
      tft.setCursor(MARGIN, 166);
      tft.print(profile->plugControlEnabled ? "Base sends batch target to Plug" : "Monitor only, relay remains off");
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_YEAST:
      if (profile->autoModeEnabled) {
        drawFrame(tft, "Yeast Preset");
        drawDropdownSelector(tft, "Preset", profile->selectedYeastPresetName, profile->yeastCategory);
      } else {
        drawFrame(tft, "Yeast Name");
        VirtualKeyboardInput::drawText(tft, editBuffer);
        drawYeastHistory(tft);
      }
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_ATTENUATION:
      drawFrame(tft, "Apparent Attenuation (%)");
      drawAttenuationList(tft);
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_DIACETYL:
      drawFrame(tft, "Diacetyl Rest");
      tft.setTextColor(uiColorTextSecondary);
      tft.setFreeFont(FONT_SIZE_SM);
      tft.setCursor(MARGIN, 132);
      tft.print("Enable rest reminder");
      IOSSwitch::draw(tft, UI_W - MARGIN - 74, 104, profile->diacetylRestEnabled);
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_YEAST_BEHAVIOR:
      drawFrame(tft, "Yeast Behavior");
      drawPresetSummary(tft);
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_TEMP_ADVANCED:
      drawFrame(tft, "Brew Profile");
      drawTemperatureAdvanced(tft);
      drawNav(tft, true, true, "NEXT");
      break;
    case WIZARD_REVIEW:
      drawFrame(tft, "Review Batch");
      tft.setTextColor(uiColorTextPrimary);
      tft.setFreeFont(FONT_SIZE_SM_BOLD);
      tft.setCursor(MARGIN, 88);
      tft.print(profile->batchName);
      tft.setFreeFont(FONT_SIZE_SM);
      tft.setTextColor(uiColorTextSecondary);
      tft.setCursor(MARGIN, 112);
      tft.printf("%s | %.1f L | %.1f Brix", profile->beerStyle, profile->batchSizeLiters, profile->recipeBrix);
      tft.setCursor(MARGIN, 138);
      tft.printf("OG %.3f  FG %.3f  ABV %.1f%%", profile->recipeOG, profile->expectedFinalGravity, profile->estimatedABV);
      tft.setCursor(MARGIN, 164);
      tft.printf("%s | Atten %d%%", profile->yeastName, profile->expectedApparentAttenuation);
      tft.setCursor(MARGIN, 190);
      tft.printf("Temp %.1f -> %.1f C, Ramp %.1f K/h",
                 profile->temperatureProfile.advanced.pitchC,
                 profile->temperatureProfile.advanced.mainC,
                 profile->temperatureProfile.advanced.normalRampKPerH);
      tft.setCursor(MARGIN, 216);
      tft.printf("D-rest %s | Crash %s",
                 profile->temperatureProfile.advanced.dRestHoldHours > 0 ? "On" : "N/A",
                 profile->temperatureProfile.advanced.crashHoldHours > 0 ? "On" : "N/A");
      tft.setCursor(MARGIN, 242);
      if (profile->dryHopEnabled) tft.printf("Dry hop +%.3f SG, %lu h", profile->dryHopFgOffset, profile->dryHopContactHours);
      else tft.print("Dry hop N/A");
      tft.setCursor(MARGIN + 230, 242);
      tft.printf("Plug %s", profile->plugControlEnabled ? "On" : "Off");
      drawNav(tft, true, true, "START");
      break;
    default:
      break;
  }
  drawCancelConfirmation(tft);
}

bool BrewWizardController::handleCommonNav(int x, int y) {
  if (confirmCancel) {
    int dialogX = MARGIN + 28;
    int dialogY = 86;
    int dialogW = UI_W - (MARGIN + 28) * 2;
    if (hitRect(x, y, dialogX + 14, dialogY + 68, 116, 38)) {
      confirmCancel = false;
      return true;
    }
    if (hitRect(x, y, dialogX + dialogW - 130, dialogY + 68, 116, 38)) {
      confirmCancel = false;
      isCancelled = true;
      return true;
    }
    return true;
  }
  if (hitRect(x, y, MARGIN - 6, WIZ_NAV_Y - 8, 136, 48)) {
    previousStep();
    return true;
  }
  if (hitRect(x, y, UI_W - MARGIN - 146, WIZ_NAV_Y - 8, 152, 48)) {
    nextStep();
    return true;
  }
  return false;
}

bool BrewWizardController::handleTouch(int x, int y) {
  if (!profile) return false;
  if (handleCommonNav(x, y)) return true;

  switch (step) {
    case WIZARD_AUTO_MODE:
      if (IOSSwitch::hit(x, y, UI_W - MARGIN - 74, 86)) {
        profile->autoModeEnabled = !profile->autoModeEnabled;
        if (profile->autoModeEnabled) {
          applySelectedPreset();
        } else {
          strcpy(profile->attenuationSource, "manual");
        }
        return true;
      }
      break;
    case WIZARD_PLUG_CONTROL:
      if (IOSSwitch::hit(x, y, UI_W - MARGIN - 74, 86)) {
        profile->plugControlEnabled = !profile->plugControlEnabled;
        return true;
      }
      break;
    case WIZARD_BATCH_NAME:
      return VirtualKeyboardInput::handleTextTouch(x, y, editBuffer, sizeof(editBuffer), &editPristine);
    case WIZARD_YEAST:
      if (profile->autoModeEnabled) {
        if (hitRect(x, y, MARGIN, WIZ_DROPDOWN_Y, 58, WIZ_DROPDOWN_H)) {
          selectPresetOffset(-1);
          return true;
        }
        if (hitRect(x, y, UI_W - MARGIN - 58, WIZ_DROPDOWN_Y, 58, WIZ_DROPDOWN_H)) {
          selectPresetOffset(1);
          return true;
        }
        return false;
      }
      for (int i = 0; i < yeastHistoryCount; i++) {
        int bx = MARGIN + 210 + i * (80 + 6);
        if (hitRect(x, y, bx, 232, 80, WIZ_KEY_H)) {
          strncpy(editBuffer, yeastHistory[i], sizeof(editBuffer) - 1);
          editBuffer[sizeof(editBuffer) - 1] = '\0';
          editPristine = false;
          return true;
        }
      }
      return VirtualKeyboardInput::handleTextTouch(x, y, editBuffer, sizeof(editBuffer), &editPristine);
    case WIZARD_BATCH_SIZE:
    case WIZARD_BRIX:
      return VirtualKeyboardInput::handleNumberTouch(x, y, editBuffer, sizeof(editBuffer), &editPristine);
    case WIZARD_BEER_STYLE:
      if (hitRect(x, y, MARGIN, WIZ_DROPDOWN_Y, 58, WIZ_DROPDOWN_H)) {
        selectStyleOffset(-1);
        return true;
      }
      if (hitRect(x, y, UI_W - MARGIN - 58, WIZ_DROPDOWN_Y, 58, WIZ_DROPDOWN_H)) {
        selectStyleOffset(1);
        return true;
      }
      break;
    case WIZARD_ATTENUATION: {
      int startX = MARGIN;
      int startY = 88;
      int w = 66;
      int h = 32;
      for (int i = 0; i < AttenuationDropdown::count(); i++) {
        int col = i % 6;
        int row = i / 6;
        if (hitRect(x, y, startX + col * (w + 9), startY + row * (h + 10), w, h)) {
          profile->expectedApparentAttenuation = AttenuationDropdown::valueAt(i);
          strcpy(profile->attenuationSource, "manual");
          profile->expectedFinalGravity = DerivedCalculations::expectedFG(profile->effectiveOG, profile->expectedApparentAttenuation);
          return true;
        }
      }
      break;
    }
    case WIZARD_DIACETYL:
      if (IOSSwitch::hit(x, y, UI_W - MARGIN - 74, 104)) {
        profile->diacetylRestEnabled = !profile->diacetylRestEnabled;
        TemperatureProfileEngine::generateForProfile(profile);
        return true;
      }
      break;
    case WIZARD_TEMP_ADVANCED:
      return handleTemperatureAdvancedTouch(x, y);
    default:
      break;
  }
  return false;
}
