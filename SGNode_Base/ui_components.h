#ifndef UI_COMPONENTS_H
#define UI_COMPONENTS_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "ui_tokens.h"

// Top bar component
// Draws top navigation bar with title and status indicators
// title: Top bar title text
// espNowOk: ESP-NOW connectivity status
// sdOk: SD card status
// battPercent: Battery percentage (0-100)
void uiDrawTopbar(const char* title, bool espNowOk, bool sdOk, uint8_t battPercent);

// Bottom navigation component
// Draws bottom navigation bar with 4 tabs
// activeTab: Currently active tab index (0-3)
// Tabs: LIVE, GRAPH, DASHBOARD, MORE with icons and underline
void uiDrawBottomNav(int activeTab);

// Navigation hit test - returns which tab was touched
// x, y: Touch coordinates
// Returns: Tab index (0-3) or -1 if no tab hit
int uiNavHitTest(int x, int y);

// Card component
// Draws a rounded rectangle card
// x, y: Position
// w, h: Dimensions
// r: Corner radius
void uiCard(int x, int y, int w, int h, int r);

// Tile component
// Draws a data tile with icon, label, value, and unit
// x, y: Position
// w, h: Dimensions
// icon: Icon identifier (placeholder for now)
// label: Label text
// value: Value text
// unit: Unit text
// muted: Whether to display in muted style
void uiTile(int x, int y, int w, int h, int icon, const char* label, 
            const char* value, const char* unit, bool muted);

// Hero SG component
// Draws a large SG display with trend and sparkline
// x, y: Position
// w, h: Dimensions
// sgValue: Current SG value
// trendText: Trend description (e.g., "+0.003" or "-0.001")
// sparklineData: Array of SG values for sparkline (last ~20 points)
// sparklineCount: Number of data points in sparkline array
void uiHeroSG(int x, int y, int w, int h, float sgValue, const char* trendText,
              float* sparklineData = NULL, int sparklineCount = 0);

// Sparkline helper
// Draws a tiny sparkline graph
// x, y: Position
// w, h: Dimensions
// data: Array of data points
// count: Number of data points
void uiDrawSparkline(int x, int y, int w, int h, float* data, int count);

// Text helper: center text within bounds
// x, y: Position
// w, h: Dimensions to center within
// text: Text to draw
// font: FreeFont pointer
// color: Text color
void uiTextCenter(int x, int y, int w, int h, const char* text, const GFXfont* font, uint16_t color);

// Text helper: right-align text within bounds
// x, y: Position
// w, h: Dimensions to right-align within
// text: Text to draw
// font: FreeFont pointer
// color: Text color
void uiTextRight(int x, int y, int w, int h, const char* text, const GFXfont* font, uint16_t color);

// Text helper: ellipsize text to fit within width
// text: Text to ellipsize
// maxWidth: Maximum width
// buffer: Buffer to store ellipsized text
// bufferSize: Size of buffer
void uiEllipsize(const char* text, int maxWidth, char* buffer, int bufferSize);

#endif // UI_COMPONENTS_H
