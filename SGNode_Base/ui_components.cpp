#include "ui_components.h"
#include <TFT_eSPI.h>
#include <string.h>

// External references to TFT and theme from main sketch
extern TFT_eSPI tft;
extern struct Theme {
  uint16_t background;
  uint16_t cardBackground;
  uint16_t buttonInactive;
  uint16_t primary;
  uint16_t primaryText;
  uint16_t accent;
  uint16_t accentText;
  uint16_t gold;
  uint16_t textPrimary;
  uint16_t textSecondary;
  uint16_t textMuted;
  uint16_t success;
  uint16_t warning;
  uint16_t error;
  uint16_t info;
  uint16_t border;
  uint16_t gridLine;
  uint16_t graphPurple;
  uint16_t graphBlue;
  uint16_t graphGreen;
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

static uint16_t mix565(uint16_t a, uint16_t b, uint8_t weightB) {
  // weightB: 0..255
  uint8_t ar = (a >> 11) & 0x1F;
  uint8_t ag = (a >> 5) & 0x3F;
  uint8_t ab = a & 0x1F;
  uint8_t br = (b >> 11) & 0x1F;
  uint8_t bg = (b >> 5) & 0x3F;
  uint8_t bb = b & 0x1F;

  uint8_t r = (uint8_t)(((uint16_t)ar * (255 - weightB) + (uint16_t)br * weightB) / 255);
  uint8_t g = (uint8_t)(((uint16_t)ag * (255 - weightB) + (uint16_t)bg * weightB) / 255);
  uint8_t bl = (uint8_t)(((uint16_t)ab * (255 - weightB) + (uint16_t)bb * weightB) / 255);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

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
  tft.fillRect(0, 0, UI_W, TOPBAR_H, uiColorPrimary);
  
  tft.setTextColor(uiColorPrimaryText);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN, TOPBAR_H / 2 + 4);
  tft.print("SGNode");

  if (title != NULL && title[0] != '\0' && strcmp(title, "SGNode") != 0) {
    char viewTitle[28];
    uiEllipsize(title, 160, viewTitle, sizeof(viewTitle));
    tft.setFreeFont(FONT_SIZE_XS);
    tft.setCursor(MARGIN + 92, TOPBAR_H / 2 + 4);
    tft.print(viewTitle);
  }
  
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
  const char* tabLabels[] = {"LIVE", "GRAPH", "DASHBOARD", ""};
  
  for (int i = 0; i < TAB_COUNT; i++) {
    int tabX = i * tabWidth;
    int centerX = tabX + tabWidth / 2;
    if (i == 1) centerX -= 8;   // Keep visual breathing room to DASHBOARD
    if (i == 2) centerX += 10;  // Keep visual breathing room from GRAPH
    
    // Remove icon circles - cleaner look
    
    if (i == 3) {
      // Draw hamburger menu icon manually (3 horizontal lines) - double size
      uint16_t iconColor = (i == activeTab) ? uiColorInfo : uiColorTextMuted;
      int iconY = navY + 12;
      int iconX = centerX - 16;
      int lineLength = 32;
      int lineSpacing = 8;
      
      tft.drawFastHLine(iconX, iconY, lineLength, iconColor);
      tft.drawFastHLine(iconX, iconY + lineSpacing, lineLength, iconColor);
      tft.drawFastHLine(iconX, iconY + lineSpacing * 2, lineLength, iconColor);
    } else {
      // Draw label centered using textWidth
      tft.setTextColor(i == activeTab ? uiColorInfo : uiColorTextMuted);
      tft.setFreeFont(FONT_SIZE_SM);
      int labelWidth = tft.textWidth(tabLabels[i]);
      int labelX = centerX - labelWidth / 2;
      tft.setCursor(labelX, navY + 29);  // shared baseline for all three labels
      tft.print(tabLabels[i]);
    }
    
    // Draw blue underline for active tab (thin bar)
    if (i == activeTab) {
      tft.drawFastHLine(tabX + 20, navY + NAV_H - 3, tabWidth - 40, uiColorInfo);
    }
  }
}

void uiCard(int x, int y, int w, int h, int r) {
  // Draw rounded card background
  tft.fillRoundRect(x, y, w, h, r, uiColorCardBackground);
  // Modern flat surface with subtle L-accent (top + left) and soft corner.
  if (w > 10 && h > 10) {
    uint16_t accent = mix565(uiColorCardBackground, uiColorTextPrimary, 28);
    uint16_t bottomShade = mix565(uiColorCardBackground, uiColorBackground, 30);
    int inset = 3;
    int corner = r > 3 ? r - 2 : 2;

    // Top accent
    tft.drawFastHLine(x + inset + corner, y + inset, w - (inset + corner) - inset, accent);
    // Left accent
    tft.drawFastVLine(x + inset, y + inset + corner, h - (inset + corner) - inset, accent);
    // Rounded top-left join (quarter arc) instead of 45-degree angle
    tft.drawCircleHelper(x + inset + corner, y + inset + corner, corner, 0x1, accent);

    // Subtle grounding shadow at bottom edge
    tft.drawFastHLine(x + 5, y + h - 3, w - 10, bottomShade);
  }
}

void uiTile(int x, int y, int w, int h, int icon, const char* label, 
            const char* value, const char* unit, bool muted) {
  // Draw rounded card with border
  uiCard(x, y, w, h, CARD_RADIUS);
  
  // Draw label (small, above value) - adjusted +6px to compensate for tile lift
  tft.setTextColor(muted ? uiColorTextMuted : uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(x + 16, y + 25);
  tft.print(label);
  
  // Draw value (larger, prominent) centered against its own font baseline
  tft.setTextColor(muted ? uiColorTextMuted : uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_MD);
  int valueFontH = tft.fontHeight();
  int valueBaselineY = y + h / 2 + valueFontH / 2 - 2;
  tft.setCursor(x + 16, valueBaselineY);
  tft.print(value);
  
  // Draw unit (small, right-aligned using textWidth)
  if (unit != NULL && strlen(unit) > 0) {
    tft.setTextColor(muted ? uiColorTextMuted : uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int unitWidth = tft.textWidth(unit);
    int unitFontH = tft.fontHeight();
    int unitBaselineY = y + h / 2 + unitFontH / 2 - 1;
    tft.setCursor(x + w - unitWidth - 10, unitBaselineY);
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
  
  // Large SG value in textPrimary (lowered by 4px)
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_XL);
  char sgBuf[16];
  snprintf(sgBuf, sizeof(sgBuf), "%.3f", sgValue);
  tft.setCursor(x + 20, y + 59);
  tft.print(sgBuf);
  
  // Draw trend pill if trend text provided (moved below SG value)
  if (trendText != NULL && strlen(trendText) > 0) {
    // Determine color based on trend direction (green for negative/down, red for positive/up)
    uint16_t trendColor = uiColorError;  // Red for positive (up slope)
    if (trendText[0] == '-') {
      trendColor = uiColorSuccess;  // Green for negative (down slope)
    }

    // Calculate trend text position (below SG value, aligned left)
    tft.setFreeFont(FONT_SIZE_SM);
    int trendTextW = tft.textWidth(trendText);
    int trendTextH = tft.fontHeight();
    int trendX = x + 20;
    int padX = 8;
    int pillH = 26;
    int pillW = trendTextW + padX * 2;
    if (pillW < 64) pillW = 64;
    int pillX = trendX - padX;
    int pillY = y + 68;  // fixed anchor below SG value

    // Draw trend background pill
    tft.fillRoundRect(pillX, pillY, pillW, pillH, 5, uiColorCardBackground);
    tft.drawRoundRect(pillX, pillY, pillW, pillH, 5, trendColor);

    // Draw trend text
    tft.setTextColor(trendColor);
    int trendBaselineY = pillY + ((pillH - trendTextH) / 2) + trendTextH - 5;
    tft.setCursor(trendX, trendBaselineY);
    tft.print(trendText);
  }
  
  // Draw sparkline showing last ~20 SG readings (right 2/3 of tile)
  if (sparklineData != NULL && sparklineCount >= 2) {
    int sparkW = (w - 40) * 2 / 3;  // Width of sparkline area (right 2/3)
    int sparkH = h - 20;  // Height of sparkline (increased by 60px)
    int sparkX = x + 20 + (w - 40) / 3;  // Start at right 1/3 margin
    int sparkY = y + h - sparkH - 10;  // Moved 60px higher
    
    uiDrawSparkline(sparkX, sparkY, sparkW, sparkH, sparklineData, sparklineCount);
  }
}

void uiDrawSparkline(int x, int y, int w, int h, float* data, int count) {
  // Draw sparkline background
  tft.fillRect(x, y, w, h, uiColorCardBackground);
  
  if (data == NULL || count < 2) {
    // Draw placeholder line
    tft.drawFastHLine(x, y + h / 2, w, currentTheme->gridLine);
    return;
  }

  for (int i = 1; i < 4; i++) {
    int gy = y + (h * i / 4);
    tft.drawFastHLine(x, gy, w, currentTheme->gridLine);
  }
  
  // Find min/max for scaling
  float minVal = data[0];
  float maxVal = data[0];
  for (int i = 1; i < count; i++) {
    if (data[i] < minVal) minVal = data[i];
    if (data[i] > maxVal) maxVal = data[i];
  }
  
  float range = maxVal - minVal;
  const float minVisualRange = 0.006f;
  if (range < minVisualRange) {
    float mid = (maxVal + minVal) * 0.5f;
    minVal = mid - minVisualRange * 0.5f;
    maxVal = mid + minVisualRange * 0.5f;
    range = minVisualRange;
  }
  
  // Draw sparkline
  int prevX = -1, prevY = -1;
  for (int i = 0; i < count; i++) {
    int px = x + (i * w / (count - 1));
    int py = y + h - ((data[i] - minVal) / range * h);
    
    if (prevX >= 0 && prevY >= 0) {
      tft.drawLine(prevX, prevY, px, py, uiColorGold);
      tft.drawLine(prevX, prevY + 1, px, py + 1, uiColorGold);
    }
    
    prevX = px;
    prevY = py;
  }

  tft.fillCircle(prevX, prevY, 3, uiColorGold);
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
