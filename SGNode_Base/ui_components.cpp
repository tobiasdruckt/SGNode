#include "ui_components.h"
#include <TFT_eSPI.h>

// External references to TFT and theme from main sketch
extern TFT_eSPI tft;
extern struct Theme {
  uint16_t background;
  uint16_t cardBackground;
  uint16_t textPrimary;
  uint16_t textSecondary;
  uint16_t textMuted;
  uint16_t border;
  uint16_t primary;
  uint16_t primaryText;
  uint16_t accent;
  uint16_t success;
  uint16_t error;
  uint16_t warning;
  uint16_t info;
  uint16_t buttonInactive;
  uint16_t gold;
} *currentTheme;

// UI color token implementations
uint16_t uiColorPrimary;
uint16_t uiColorPrimaryText;
uint16_t uiColorBackground;
uint16_t uiColorCardBackground;
uint16_t uiColorTextPrimary;
uint16_t uiColorTextSecondary;
uint16_t uiColorTextMuted;
uint16_t uiColorBorder;
uint16_t uiColorAccent;
uint16_t uiColorSuccess;
uint16_t uiColorError;
uint16_t uiColorWarning;
uint16_t uiColorInfo;
uint16_t uiColorGold;

void uiInitColors() {
  // Initialize UI color tokens from current theme
  uiColorPrimary = currentTheme->primary;
  uiColorPrimaryText = currentTheme->primaryText;
  uiColorBackground = currentTheme->background;
  uiColorCardBackground = currentTheme->cardBackground;
  uiColorTextPrimary = currentTheme->textPrimary;
  uiColorTextSecondary = currentTheme->textSecondary;
  uiColorTextMuted = currentTheme->textMuted;
  uiColorBorder = currentTheme->border;
  uiColorAccent = currentTheme->accent;
  uiColorSuccess = currentTheme->success;
  uiColorError = currentTheme->error;
  uiColorWarning = currentTheme->warning;
  uiColorInfo = currentTheme->info;  // Blue for SD status
  uiColorGold = currentTheme->gold;
}

void uiDrawTopbar(const char* title, bool espNowOk, bool sdOk, uint8_t battPercent) {
  // Draw top bar background with navy color
  tft.fillRect(0, 0, UI_W, TOPBAR_H, uiColorPrimary);  // Using primary (navy)
  
  // Draw title "SGNode" (left side)
  tft.setTextColor(uiColorPrimaryText);
  tft.setFreeFont(FONT_SIZE_SM);  // Use smaller font
  tft.setCursor(MARGIN, TOPBAR_H / 2 + 4);  // Adjust position
  tft.print("SGNode");
  
  // Draw battery icon + percentage on the right side
  int battW = 24;                  // Battery body width
  int battH = 12;                  // Battery body height
  int battX = UI_W - MARGIN - battW - 4;  // Battery icon at far right
  int battY = TOPBAR_H / 2 - 6;    // Centered vertically
  
  // Battery body (outline)
  uint16_t battColor;
  if (battPercent > 50) battColor = uiColorSuccess;
  else if (battPercent > 20) battColor = uiColorWarning;
  else battColor = uiColorError;
  
  tft.drawRect(battX, battY, battW, battH, uiColorPrimaryText);
  tft.fillRect(battX + 2, battY + 2, (battW - 4) * battPercent / 100, battH - 4, battColor);
  
  // Battery terminal (nub on right)
  tft.fillRect(battX + battW, battY + 3, 3, 6, uiColorPrimaryText);
  
  // Battery percentage text (positioned left of the battery icon)
  tft.setTextColor(uiColorPrimaryText);
  tft.setFreeFont(FONT_SIZE_SM);
  char battBuf[8];
  snprintf(battBuf, sizeof(battBuf), "%d%%", battPercent);
  int textW = tft.textWidth(battBuf);
  tft.setCursor(battX - textW - 8, TOPBAR_H / 2 + 4);  // Position left of battery icon
  tft.print(battBuf);
}

void uiRedrawTopbar() {
  // Redraw only the topbar region - called for status updates
  // Note: Need external data for status - this is a placeholder
  // In practice, the caller should use uiDrawTopbar with current status
  tft.fillRect(0, 0, UI_W, TOPBAR_H, uiColorPrimary);
}

int uiNavHitTest(int x, int y) {
  int navY = UI_H - NAV_H;
  if (y < navY) return -1;  // Not in nav area
  
  int tabWidth = UI_W / TAB_COUNT;
  int tab = x / tabWidth;
  if (tab < 0 || tab >= TAB_COUNT) return -1;
  return tab;
}

void uiDrawBottomNav(int activeTab) {
  int navY = UI_H - NAV_H;
  int tabWidth = UI_W / TAB_COUNT;
  
  // Draw navigation background
  tft.fillRect(0, navY, UI_W, NAV_H, uiColorCardBackground);
  
  // Tab labels
  const char* tabLabels[] = {"LIVE", "GRAPH", "CALIB", ""};
  
  for (int i = 0; i < TAB_COUNT; i++) {
    int tabX = i * tabWidth;
    int centerX = tabX + tabWidth / 2;
    
    // Remove icon circles - cleaner look
    
    if (i == 3) {
      // Draw hamburger menu icon manually (3 horizontal lines) - double size
      uint16_t iconColor = (i == activeTab) ? COLOR_BLUE : COLOR_MUTED;
      int iconY = navY + 12;
      int iconX = centerX - 16;
      int lineLength = 32;
      int lineSpacing = 8;
      
      tft.drawFastHLine(iconX, iconY, lineLength, iconColor);
      tft.drawFastHLine(iconX, iconY + lineSpacing, lineLength, iconColor);
      tft.drawFastHLine(iconX, iconY + lineSpacing * 2, lineLength, iconColor);
    } else {
      // Draw label centered using textWidth
      tft.setTextColor(i == activeTab ? COLOR_BLUE : COLOR_MUTED);
      tft.setFreeFont(FONT_SIZE_SM);
      int labelWidth = tft.textWidth(tabLabels[i]);
      int labelX = tabX + (tabWidth - labelWidth) / 2;
      tft.setCursor(labelX, navY + 28);  // Adjust for FreeFont baseline
      tft.print(tabLabels[i]);
    }
    
    // Draw blue underline for active tab (thin bar)
    if (i == activeTab) {
      tft.drawFastHLine(tabX + 20, navY + NAV_H - 3, tabWidth - 40, COLOR_BLUE);
    }
  }
}

void uiCard(int x, int y, int w, int h, int r) {
  // Draw rounded card background
  tft.fillRoundRect(x, y, w, h, r, uiColorCardBackground);
  // Draw rounded border
  tft.drawRoundRect(x, y, w, h, r, uiColorBorder);
}

void uiTile(int x, int y, int w, int h, int icon, const char* label, 
            const char* value, const char* unit, bool muted) {
  // Draw rounded card with border
  uiCard(x, y, w, h, CARD_RADIUS);
  
  // Draw icon placeholder (minimal circle)
  tft.fillCircle(x + 20, y + h / 2, ICON_SIZE_SM / 2, 
                muted ? uiColorTextMuted : uiColorAccent);
  
  // Draw label (small, above value)
  tft.setTextColor(muted ? uiColorTextMuted : uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(x + 40, y + 18);  // Adjust for FreeFont baseline
  tft.print(label);
  
  // Draw value (larger, prominent) - centered in tile
  tft.setTextColor(muted ? uiColorTextMuted : uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_MD);
  int valueY = y + h / 2 + 6;  // Center vertically
  tft.setCursor(x + 40, valueY);
  tft.print(value);
  
  // Draw unit (small, right-aligned using textWidth)
  if (unit != NULL && strlen(unit) > 0) {
    tft.setTextColor(muted ? uiColorTextMuted : uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int unitWidth = tft.textWidth(unit);
    tft.setCursor(x + w - unitWidth - 10, valueY);  // Align with value
    tft.print(unit);
  }
}

void uiHeroSG(int x, int y, int w, int h, float sgValue, const char* trendText,
              float* sparklineData, int sparklineCount) {
  // Draw white card with border (no navy fill)
  uiCard(x, y, w, h, CARD_RADIUS);
  
  // Small "SG" label in muted
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(x + 20, y + 22);
  tft.print("SG");
  
  // Large SG value in textPrimary
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_XL);
  char sgBuf[16];
  snprintf(sgBuf, sizeof(sgBuf), "%.3f", sgValue);
  tft.setCursor(x + 20, y + 55);
  tft.print(sgBuf);
  
  // Draw trend pill if trend text provided
  if (trendText != NULL && strlen(trendText) > 0) {
    // Determine color based on trend direction
    uint16_t trendColor = uiColorGold;  // Default gold for positive (fermentation)
    if (trendText[0] == '-') {
      trendColor = uiColorSuccess;  // Green for negative/stable
    }
    
    // Calculate trend text position (right side of hero)
    int trendX = x + w - 80;
    int trendY = y + 35;
    
    // Draw trend background pill
    tft.fillRoundRect(trendX - 5, trendY - 12, 70, 24, 4, uiColorCardBackground);
    tft.drawRoundRect(trendX - 5, trendY - 12, 70, 24, 4, trendColor);
    
    // Draw trend text
    tft.setTextColor(trendColor);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(trendX, trendY + 4);
    tft.print(trendText);
  }
  
  // Draw sparkline showing last ~20 SG readings
  if (sparklineData != NULL && sparklineCount >= 2) {
    int sparkW = w - 40;  // Width of sparkline area
    int sparkH = 25;      // Height of sparkline
    int sparkX = x + 20;  // Left margin
    int sparkY = y + h - sparkH - 10;  // Bottom margin
    
    uiDrawSparkline(sparkX, sparkY, sparkW, sparkH, sparklineData, sparklineCount);
  }
}

void uiDrawSparkline(int x, int y, int w, int h, float* data, int count) {
  // Draw sparkline background
  tft.fillRect(x, y, w, h, uiColorBackground);
  
  if (data == NULL || count < 2) {
    // Draw placeholder line
    tft.drawFastHLine(x, y + h / 2, w, uiColorTextMuted);
    return;
  }
  
  // Find min/max for scaling
  float minVal = data[0];
  float maxVal = data[0];
  for (int i = 1; i < count; i++) {
    if (data[i] < minVal) minVal = data[i];
    if (data[i] > maxVal) maxVal = data[i];
  }
  
  float range = maxVal - minVal;
  if (range == 0) range = 1;
  
  // Draw sparkline
  int prevX = -1, prevY = -1;
  for (int i = 0; i < count; i++) {
    int px = x + (i * w / (count - 1));
    int py = y + h - ((data[i] - minVal) / range * h);
    
    if (prevX >= 0 && prevY >= 0) {
      tft.drawLine(prevX, prevY, px, py, uiColorAccent);
    }
    
    prevX = px;
    prevY = py;
  }
}

void uiTextCenter(int x, int y, int w, int h, const char* text, const GFXfont* font, uint16_t color) {
  tft.setTextColor(color);
  tft.setFreeFont(font);
  int textWidth = tft.textWidth(text);
  int textHeight = tft.fontHeight();
  // FreeFont baseline is at bottom, so adjust Y to center the text properly
  int startX = x + (w - textWidth) / 2;
  int startY = y + (h + textHeight) / 2 - 2;  // Adjust for baseline
  tft.setCursor(startX, startY);
  tft.print(text);
}

void uiTextRight(int x, int y, int w, int h, const char* text, const GFXfont* font, uint16_t color) {
  tft.setTextColor(color);
  tft.setFreeFont(font);
  int textWidth = tft.textWidth(text);
  int textHeight = tft.fontHeight();
  // FreeFont baseline is at bottom, so adjust Y to align properly
  int startX = x + w - textWidth;
  int startY = y + (h + textHeight) / 2 - 2;  // Adjust for baseline
  tft.setCursor(startX, startY);
  tft.print(text);
}

void uiEllipsize(const char* text, int maxWidth, char* buffer, int bufferSize) {
  tft.setFreeFont(FONT_SIZE_SM);  // Measure at small font for consistency
  int textWidth = tft.textWidth(text);
  if (textWidth <= maxWidth) {
    strncpy(buffer, text, bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return;
  }
  
  // Calculate max characters that fit
  int charWidth = tft.textWidth("X");  // Width of one character
  int ellipsisWidth = tft.textWidth("…");
  int maxChars = (maxWidth - ellipsisWidth) / charWidth;
  
  if (maxChars < 1) {
    strncpy(buffer, "…", bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return;
  }
  
  strncpy(buffer, text, min(maxChars, bufferSize - 2));
  buffer[min(maxChars, bufferSize - 2)] = '\0';
  strncat(buffer, "…", bufferSize - strlen(buffer) - 1);
}
