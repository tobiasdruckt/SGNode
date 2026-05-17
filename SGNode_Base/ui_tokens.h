#ifndef UI_TOKENS_H
#define UI_TOKENS_H

// Layout Constants
#define UI_W            480  // Screen width
#define UI_H            320  // Screen height
#define TOPBAR_H        36   // Top bar height
#define NAV_H           44   // Bottom navigation height
#define MARGIN          14   // General margin spacing
#define GAP             12   // Gap between elements
#define CARD_RADIUS     18   // Card corner radius (ultra-clean)
#define TILE_H          80   // Standard tile height
#define HERO_H          120  // Hero section height

// Spacing tokens
#define SPACING_XS      4
#define SPACING_SM      8
#define SPACING_MD      12
#define SPACING_LG      16
#define SPACING_XL      24

// Typography tokens (FreeFont Sans Serif)
#define FONT_SIZE_XS    &FreeSans9pt7b
#define FONT_SIZE_SM    &FreeSans12pt7b
#define FONT_SIZE_SM_BOLD    &FreeSansBold12pt7b
#define FONT_SIZE_MD    &FreeSans18pt7b
#define FONT_SIZE_LG    &FreeSans24pt7b
#define FONT_SIZE_XL    &FreeSansBold24pt7b

// Icon sizes
#define ICON_SIZE_SM    16
#define ICON_SIZE_MD    24
#define ICON_SIZE_LG    32

// Tab definitions
#define TAB_COUNT       4
#define TAB_LIVE        0
#define TAB_GRAPH       1
#define TAB_BATTERY     2
#define TAB_MORE        3

// Color constants (RGB565)
#define COLOR_BG        0xF9FA  // #FAFAFA approx
#define COLOR_CARD      0xFFFF  // #FFFFFF
#define COLOR_BORDER    0xC618  // #E5E7EB approx
#define COLOR_TEXT      0x8410  // #111827 approx
#define COLOR_MUTED     0x632A  // #6B7280 approx
#define COLOR_NAVY      0x0822  // #0B1F2A approx
#define COLOR_GOLD      0xE3C4  // #C78A1A approx
#define COLOR_BLUE      0x1E96  // #2196F3 approx (info blue)

// Polynomial graph positioning constants
#define POLY_GRAPH_X      MARGIN
#define POLY_GRAPH_Y      TOPBAR_H + MARGIN
#define POLY_GRAPH_W      UI_W - MARGIN * 2
#define POLY_GRAPH_H      UI_H - TOPBAR_H - NAV_H - MARGIN * 2

// Graph dimensions within the card
#define POLY_GRAPH_INNER_X  MARGIN + 20
#define POLY_GRAPH_INNER_Y  POLY_GRAPH_Y + 40
#define POLY_GRAPH_INNER_W  POLY_GRAPH_W - 40
#define POLY_GRAPH_INNER_H  POLY_GRAPH_H - 80

// Graph axes and grid
#define POLY_GRAPH_AXIS_COLOR    COLOR_BORDER
#define POLY_GRAPH_GRID_COLOR    COLOR_MUTED
#define POLY_GRAPH_CURVE_COLOR   COLOR_GOLD

// Helper function to convert RGB to RGB565
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Style tokens (will reference currentTheme)
// These are semantic names that map to theme colors
extern uint16_t uiColorPrimary;
extern uint16_t uiColorPrimaryText;
extern uint16_t uiColorBackground;
extern uint16_t uiColorCardBackground;
extern uint16_t uiColorTextPrimary;
extern uint16_t uiColorTextSecondary;
extern uint16_t uiColorTextMuted;
extern uint16_t uiColorBorder;
extern uint16_t uiColorAccent;
extern uint16_t uiColorSuccess;
extern uint16_t uiColorError;
extern uint16_t uiColorWarning;
extern uint16_t uiColorInfo;
extern uint16_t uiColorGold;

// Initialize UI color tokens from current theme
void uiInitColors();

#endif // UI_TOKENS_H
