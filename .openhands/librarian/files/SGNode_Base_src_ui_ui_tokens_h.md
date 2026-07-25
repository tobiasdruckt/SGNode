# SGNode_Base/src/ui/ui_tokens.h

**Type:** C++ Header
**Hash:** `81e7a9e8c39ebcd3e68f7c3f386d6396a2682b14abef90656b2ec9db0fd2cd3e`

## Overview / Role

User interface components and rendering

## Verified API & Symbols

### Functions

- `void rgb565 (uint8_t r, uint8_t g, uint8_t b)`

### Macros

- `#define UI_TOKENS_H // Layout Constants`

- `#define UI_W 480  // Screen width`

- `#define UI_H 320  // Screen height`

- `#define TOPBAR_H 36   // Top bar height`

- `#define NAV_H 44   // Bottom navigation height`

- `#define MARGIN 14   // General margin spacing`

- `#define GAP 12   // Gap between elements`

- `#define CARD_RADIUS 8    // Card corner radius, compact for 4-inch UI`

- `#define HERO_H 120  // Hero section height`

- `#define SPACING_XS 4`

- `#define SPACING_SM 8`

- `#define SPACING_MD 12`

- `#define SPACING_LG 16`

- `#define SPACING_XL 24`

- `#define FONT_SIZE_XS &FreeSans9pt7b`

- `#define FONT_SIZE_SM &FreeSans12pt7b`

- `#define FONT_SIZE_SM_BOLD &FreeSansBold12pt7b`

- `#define FONT_SIZE_MD &FreeSans18pt7b`

- `#define FONT_SIZE_LG &FreeSans24pt7b`

- `#define FONT_SIZE_XL &FreeSansBold24pt7b`

- `#define ICON_SIZE_SM 16`

- `#define ICON_SIZE_MD 24`

- `#define ICON_SIZE_LG 32`

- `#define TAB_COUNT 4`

- `#define TAB_LIVE 0`

- `#define TAB_GRAPH 1`

- `#define TAB_DASHBOARD 2`

- `#define TAB_BATTERY TAB_DASHBOARD`

- `#define TAB_MORE 3`

- `#define COLOR_BG 0xF9FA  //`

- `#define COLOR_CARD 0xFFFF  //`

- `#define COLOR_BORDER 0xC618  //`

- `#define COLOR_TEXT 0x8410  //`

- `#define COLOR_MUTED 0x632A  //`

- `#define COLOR_NAVY 0x0822  //`

- `#define COLOR_GOLD 0xE3C4  //`

- `#define COLOR_BLUE 0x1E96  //`

- `#define POLY_GRAPH_X MARGIN`

- `#define POLY_GRAPH_Y TOPBAR_H + MARGIN`

- `#define POLY_GRAPH_W UI_W - MARGIN * 2`

- `#define POLY_GRAPH_H UI_H - TOPBAR_H - NAV_H - MARGIN * 2`

- `#define POLY_GRAPH_INNER_X MARGIN + 20`

- `#define POLY_GRAPH_INNER_Y POLY_GRAPH_Y + 40`

- `#define POLY_GRAPH_INNER_W POLY_GRAPH_W - 40`

- `#define POLY_GRAPH_INNER_H POLY_GRAPH_H - 80`

- `#define POLY_GRAPH_AXIS_COLOR COLOR_BORDER`

- `#define POLY_GRAPH_GRID_COLOR COLOR_MUTED`

- `#define POLY_GRAPH_CURVE_COLOR COLOR_GOLD`

## Key Dependencies

- No external dependencies

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: UI changes affect user experience

---
*Last modified (hash): 81e7a9e8c39ebcd3e68f7c3f386d6396a2682b14abef90656b2ec9db0fd2cd3e*