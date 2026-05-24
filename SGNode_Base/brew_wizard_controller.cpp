#include "brew_wizard_controller.h"
#include "brix_converter.h"
#include "derived_calculations.h"
#include "yeast_preset_repository.h"
#include "batch_action.h"
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

static bool hitRect(int px, int py, int x, int y, int w, int h) {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

static void appendChar(char* buffer, size_t bufferSize, char c, bool* clearOnInput) {
  if (clearOnInput && *clearOnInput) {
    buffer[0] = '\0';
    *clearOnInput = false;
  }
  size_t l = strlen(buffer);
  if (l + 1 < bufferSize) {
    buffer[l] = c;
    buffer[l + 1] = '\0';
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
  drawKey(tft, MARGIN, 232, 94, WIZ_KEY_H, "SPACE");
  drawKey(tft, MARGIN + 102, 232, 94, WIZ_KEY_H, "DEL");
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
  if (hitRect(x, y, MARGIN, 232, 94, WIZ_KEY_H)) {
    appendChar(buffer, bufferSize, ' ', clearOnInput);
    return true;
  }
  if (hitRect(x, y, MARGIN + 102, 232, 94, WIZ_KEY_H)) {
    if (clearOnInput && *clearOnInput) {
      buffer[0] = '\0';
      *clearOnInput = false;
      return true;
    }
    size_t l = strlen(buffer);
    if (l > 0) buffer[l - 1] = '\0';
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
  if (step == WIZARD_REVIEW) {
    isComplete = true;
    step = WIZARD_DONE;
    return;
  }
  if (profile && profile->autoModeEnabled && step == WIZARD_YEAST) {
    step = WIZARD_YEAST_BEHAVIOR;
  } else if (profile && profile->autoModeEnabled && step == WIZARD_YEAST_BEHAVIOR) {
    step = WIZARD_REVIEW;
  } else if (profile && !profile->autoModeEnabled && step == WIZARD_DIACETYL) {
    step = WIZARD_REVIEW;
  } else if (profile && profile->autoModeEnabled && step == WIZARD_AUTO_MODE) {
    step = WIZARD_YEAST;
  } else {
    step = (BrewWizardStep)((int)step + 1);
  }
  if (step == WIZARD_REVIEW && profile) {
    BatchActionEngine::applyStyleDefaults(profile);
  }
  loadStepBuffer();
}

void BrewWizardController::previousStep() {
  if (step == WIZARD_BATCH_NAME) {
    confirmCancel = true;
    return;
  }
  commitStepBuffer();
  if (profile && profile->autoModeEnabled && step == WIZARD_REVIEW) {
    step = WIZARD_YEAST_BEHAVIOR;
  } else if (profile && !profile->autoModeEnabled && step == WIZARD_REVIEW) {
    step = WIZARD_DIACETYL;
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
  WizardStepper::draw(tft, (int)step, 10, MARGIN, TOPBAR_H + 10, UI_W - MARGIN * 2);
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
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN, 94);
  tft.print(title);

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
  tft.printf("Duration: %.0f h, lag %.0f h", profile->typicalDurationHours, profile->lagPhaseHours);
  tft.setCursor(MARGIN + 12, 220);
  tft.printf("D-rest: %s", profile->diacetylRestRecommendedByYeast ? "recommended" : "not typical");
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
      drawDropdownSelector(tft, "Style", profile->beerStyle, "Use arrows to choose the beer style");
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
    case WIZARD_REVIEW:
      drawFrame(tft, "Review Batch");
      tft.setTextColor(uiColorTextPrimary);
      tft.setFreeFont(FONT_SIZE_SM);
      tft.setCursor(MARGIN, 92); tft.printf("%s / %s", profile->batchName, profile->beerStyle);
      tft.setCursor(MARGIN, 122); tft.printf("%.1f L, %.1f Brix, OG %.3f", profile->batchSizeLiters, profile->recipeBrix, profile->recipeOG);
      tft.setCursor(MARGIN, 152); tft.printf("%s, %d%% attenuation", profile->yeastName, profile->expectedApparentAttenuation);
      tft.setCursor(MARGIN, 182); tft.printf("Expected FG %.3f", profile->expectedFinalGravity);
      tft.setCursor(MARGIN, 212);
      tft.printf("%s / D-rest: %s", profile->autoModeEnabled ? "Auto" : "Manual", profile->diacetylRestEnabled ? "On" : "Off");
      tft.setTextColor(profile->dryHopEnabled ? uiColorTextPrimary : uiColorTextSecondary);
      tft.setFreeFont(FONT_SIZE_XS);
      tft.setCursor(MARGIN, 238);
      if (profile->dryHopEnabled) {
        tft.printf("Dry hop: SG %.3f, contact %lu h", profile->dryHopTriggerSG, profile->dryHopContactHours);
      } else {
        tft.print("Dry hop: Off");
      }
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
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}
