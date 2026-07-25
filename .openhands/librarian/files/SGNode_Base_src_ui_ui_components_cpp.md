# SGNode_Base/src/ui/ui_components.cpp

**Type:** C++ Source
**Hash:** `06635495c5c12434af707edcc0300564c792b81aac7252cb1646f26a57818556`

## Overview / Role

User interface components and rendering

## Verified API & Symbols

### Structs & Classes

- `struct Theme`
  - `uint16_t background`
  - `uint16_t cardBackground`
  - `uint16_t buttonInactive`
  - `uint16_t primary`
  - `uint16_t primaryText`
  - `uint16_t accent`
  - `uint16_t accentText`
  - `uint16_t gold`
  - `uint16_t textPrimary`
  - `uint16_t textSecondary`
  - `uint16_t textMuted`
  - `uint16_t success`
  - `uint16_t warning`
  - `uint16_t error`
  - `uint16_t info`
  - `uint16_t border`
  - `uint16_t gridLine`
  - `uint16_t graphPurple`
  - `uint16_t graphBlue`
  - `uint16_t graphGreen`

### Functions

- `void mix565 (uint16_t a, uint16_t b, uint8_t weightB)`

- `void uiInitColors (void)`

- `void uiDrawTopbar (const char* title, bool espNowOk, bool sdOk, uint8_t battPercent) const`

- `void uiNavHitTest (int x, int y)`

- `void uiDrawBottomNav (int activeTab)`

- `void uiCard (int x, int y, int w, int h, int r)`

- `void uiTile (int x, int y, int w, int h, int icon, const char* label, const char* value, const char* unit, bool muted) const`

- `void uiHeroSG (int x, int y, int w, int h, float sgValue, const char* trendText, float* sparklineData, int sparklineCount) const`

- `void uiDrawSparkline (int x, int y, int w, int h, float* data, int count)`

- `void uiTextCenter (int x, int y, int w, int h, const char* text, const GFXfont* font, uint16_t color) const`

- `void uiTextRight (int x, int y, int w, int h, const char* text, const GFXfont* font, uint16_t color) const`

- `void uiEllipsize (const char* text, int maxWidth, char* buffer, int bufferSize) const`

## Key Dependencies

**Local:**
- `SGNode_Base/src/ui/ui_components.h`

**System/External:**
- `<time.h>`
- `<TFT_eSPI.h>`
- `<string.h>`

## Side Effects & Hardware Access

- SD card read
- Touchscreen UI updates
- SPI bus communication

## Change Risks

- **MEDIUM**: UI changes affect user experience

---
*Last modified (hash): 06635495c5c12434af707edcc0300564c792b81aac7252cb1646f26a57818556*