/*
 * Fermentation Monitor - Base Station
 * Receives data via ESP-NOW and displays on TFT with touch interface
 * Shows real-time data and historical graphs
 * 
 * REQUIRED LIBRARIES (install via Arduino Library Manager):
 * - TFT_eSPI (by Bodmer)
 * - XPT2046_Touchscreen (by Paul Stoffregen)
 * - SD (built-in)
 * - FS (built-in)
 */

// Debug Configuration - Set debug level for compilation
// DEBUG_NONE: No debug output (production)
// DEBUG_ERROR: Only error messages
// DEBUG_INFO: Info and error messages  
// DEBUG_VERBOSE: All debug output
#define DEBUG_LEVEL DEBUG_VERBOSE

#include <WiFi.h>
#include <esp_now.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "Logo2_Optimized.h"

// Debug macros for conditional compilation
#if DEBUG_LEVEL >= DEBUG_VERBOSE
  #define DEBUG_VERBOSE_PRINT(x) Serial.print(x)
  #define DEBUG_VERBOSE_PRINTLN(x) Serial.println(x)
  #define DEBUG_VERBOSE_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_VERBOSE_PRINT(x)
  #define DEBUG_VERBOSE_PRINTLN(x)
  #define DEBUG_VERBOSE_PRINTF(...)
#endif

#if DEBUG_LEVEL >= DEBUG_INFO
  #define DEBUG_INFO_PRINT(x) Serial.print(x)
  #define DEBUG_INFO_PRINTLN(x) Serial.println(x)
  #define DEBUG_INFO_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_INFO_PRINT(x)
  #define DEBUG_INFO_PRINTLN(x)
  #define DEBUG_INFO_PRINTF(...)
#endif

#if DEBUG_LEVEL >= DEBUG_ERROR
  #define DEBUG_ERROR_PRINT(x) Serial.print(x)
  #define DEBUG_ERROR_PRINTLN(x) Serial.println(x)
  #define DEBUG_ERROR_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_ERROR_PRINT(x)
  #define DEBUG_ERROR_PRINTLN(x)
  #define DEBUG_ERROR_PRINTF(...)
#endif

// Debug level constants
#define DEBUG_NONE 0
#define DEBUG_ERROR 1
#define DEBUG_INFO 2
#define DEBUG_VERBOSE 3

// 4.0inch ESP32-32E Display configuration
#define SCREEN_W    320
#define SCREEN_H    240
#define DISPLAY_CS  5   // Chip Select (may vary based on module)
#define DISPLAY_DC  4   // Data/Command
#define DISPLAY_RST 2  // Reset

// Custom color definitions (TFT_eSPI doesn't have TFT_GRAY)
#define TFT_GRAY 0x8410  // Dark gray color

// Helper function to convert RGB to RGB565
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Theme system for UI
struct Theme {
  // Background colors
  uint16_t background;
  uint16_t cardBackground;
  uint16_t buttonInactive;
  
  // Primary colors
  uint16_t primary;
  uint16_t primaryText;
  
  // Accent colors
  uint16_t accent;
  uint16_t accentText;
  
  // Text colors
  uint16_t textPrimary;
  uint16_t textSecondary;
  uint16_t textMuted;
  
  // Status colors
  uint16_t success;
  uint16_t warning;
  uint16_t error;
  uint16_t info;
  
  // Border colors
  uint16_t border;
  uint16_t gridLine;
};

// Light mode theme (default)
Theme lightTheme = {
  .background = rgb565(255, 255, 255),      // White
  .cardBackground = rgb565(240, 240, 240), // Light gray
  .buttonInactive = rgb565(224, 224, 224), // Lighter gray
  .primary = rgb565(11, 31, 42),           // Navy
  .primaryText = rgb565(255, 255, 255),   // White
  .accent = rgb565(199, 138, 26),          // Gold
  .accentText = rgb565(255, 255, 255),     // White
  .textPrimary = rgb565(0, 0, 0),         // Black
  .textSecondary = rgb565(51, 51, 51),    // Dark gray
  .textMuted = rgb565(102, 102, 102),    // Medium gray
  .success = rgb565(76, 175, 80),         // Green
  .warning = rgb565(255, 152, 0),          // Orange
  .error = rgb565(244, 67, 54),           // Red
  .info = rgb565(33, 150, 243),           // Blue
  .border = rgb565(204, 204, 204),        // Light border
  .gridLine = rgb565(221, 221, 221)       // Light grid
};

// Dark mode theme
Theme darkTheme = {
  .background = rgb565(0, 0, 0),          // Black
  .cardBackground = rgb565(26, 26, 26),    // Dark gray
  .buttonInactive = rgb565(42, 42, 42),   // Darker gray
  .primary = rgb565(74, 144, 226),        // Light blue
  .primaryText = rgb565(255, 255, 255),   // White
  .accent = rgb565(199, 138, 26),          // Gold
  .accentText = rgb565(255, 255, 255),    // White
  .textPrimary = rgb565(255, 255, 255),   // White
  .textSecondary = rgb565(204, 204, 204), // Light gray
  .textMuted = rgb565(136, 136, 136),     // Medium gray
  .success = rgb565(76, 175, 80),         // Green
  .warning = rgb565(255, 152, 0),          // Orange
  .error = rgb565(244, 67, 54),           // Red
  .info = rgb565(33, 150, 243),           // Blue
  .border = rgb565(68, 68, 68),           // Dark border
  .gridLine = rgb565(51, 51, 51)          // Dark grid
};

// Current theme and mode
Theme* currentTheme = &lightTheme;
bool darkMode = false;

// Boot screen state
bool bootScreenComplete = false;

// Screen dirty flag for efficient display updates
bool screenDirty = true;
bool staticElementsDrawn = false;

// Touch screen pins (integrated on ESP32-32E)
#define TOUCH_CS    15
#define TOUCH_IRQ   27

// SD card pins (4.0inch ESP32-32E has built-in SD card slot)
#define SD_CS       13  // SD card chip select (may vary based on module)

// ESP-NOW configuration
#define ESPNOW_CHANNEL 1

// Data structure for received data (must match float unit)
typedef struct __attribute__((packed)) {
  uint8_t version;        // Protocol version (current: 2)
  uint16_t sequence_id;   // Rolling sequence number
  uint32_t uptime_s;      // Uptime in seconds
  float angle;
  float density;
  float temperature;
  float battery_voltage;  // Battery voltage in volts
  uint8_t flags;          // Bitfield: bit0=delayed, bit1=sensor_err, bit2=low_batt
  uint16_t crc;           // CRC16 checksum
} payload_t;

// Calibration command structure
typedef struct __attribute__((packed)) {
  uint8_t command;        // 0=CALIBRATE_POINT1, 1=CALIBRATE_POINT2, 2=CALIBRATE_POINT3, 3=CALIBRATE_POINT4, 4=APPLY_CALIBRATION
  float target_sg;        // Target specific gravity for calibration point
  uint8_t request_id;     // Unique ID for response tracking
} calib_command_t;

// Calibration response structure
typedef struct __attribute__((packed)) {
  uint8_t response_type;  // 0=CALIBRATION_DATA, 1=ACK, 2=ERROR
  float angle;           // Measured angle
  float sg;              // Calculated SG
  uint8_t request_id;    // Echo back request ID
  char message[32];       // Status message
} calib_response_t;

// Data buffer for historical data
#define MAX_DATA_POINTS 100
payload_t dataBuffer[MAX_DATA_POINTS];
int dataIndex = 0;
int dataCount = 0;

// ABV calculation variables
float originalGravity = 0.0;  // OG captured when stable
float currentABV = 0.0;       // Calculated ABV
int readingCount = 0;         // Number of readings since fermentation started
float last3Readings[3] = {0.0, 0.0, 0.0}; // Last 3 SG readings for stability check
bool ogCaptured = false;      // Whether OG has been captured
#define OG_STABILITY_THRESHOLD 0.001 // SG variance threshold for stability
#define OG_MAX_READINGS 10     // Force capture at this reading if not stable

// Display objects
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// UI state
enum DisplayMode {
  LIVE_VIEW,
  GRAPH_VIEW,
  CALIBRATION_VIEW
};

DisplayMode currentMode = LIVE_VIEW;

// Calibration state
enum CalibMode {
  CALIB_IDLE,
  CALIB_INSTRUCTIONS,
  CALIB_POINT1,
  CALIB_POINT2,
  CALIB_POINT3,
  CALIB_POINT4,
  CALIB_COMPLETE
};

CalibMode calibMode = CALIB_IDLE;
float calibAngles[4] = {0.0, 0.0, 0.0, 0.0}; // Store angles for 4 calibration points
float calibSG[4] = {1.000, 1.040, 1.080, 1.120}; // Target SG values for 4 points
int sugarAmounts[4] = {0, 80, 80, 80}; // Sugar to add at each step (grams) - for 2L water
uint8_t currentRequestId = 0;
bool calibrationModeActive = false; // Track if float is in calibration mode

// Touch handling
bool touchPressed = false;
uint16_t touchX, touchY;

// Timing
unsigned long lastUpdate = 0;
unsigned long lastTouchCheck = 0;
const unsigned long TOUCH_CHECK_INTERVAL = 50; // ms

// Button areas for touch interface
#define BUTTON_LIVE_X 10
#define BUTTON_LIVE_Y 10
#define BUTTON_LIVE_W 60
#define BUTTON_LIVE_H 30

#define BUTTON_GRAPH_X 80
#define BUTTON_GRAPH_Y 10
#define BUTTON_GRAPH_W 60
#define BUTTON_GRAPH_H 30

#define BUTTON_CALIB_X 150
#define BUTTON_CALIB_Y 10
#define BUTTON_CALIB_W 80
#define BUTTON_CALIB_H 30

// Reboot prompt buttons
#define BUTTON_CONTINUE_X 60
#define BUTTON_CONTINUE_Y 140
#define BUTTON_CONTINUE_W 100
#define BUTTON_CONTINUE_H 40

#define BUTTON_NEW_X 160
#define BUTTON_NEW_Y 140
#define BUTTON_NEW_W 100
#define BUTTON_NEW_H 40

char currentFermentationFile[32] = "";
bool fermentationFileOpen = false;
bool rebootPromptShown = false;
bool waitingForRebootChoice = false;
bool sdCardInitialized = false;
bool fermentationCheckDone = false;
bool sdErrorMessageShown = false;
unsigned long bootScreenCompleteTime = 0;
#define SD_ERROR_DELAY 2000 // 2 seconds delay after boot screen

// ESP-NOW peer management
uint8_t lastFloatMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool floatPeerRegistered = false;

// Calibration buttons (step-by-step wizard)
#define BUTTON_CALIB_START_X 110
#define BUTTON_CALIB_START_Y 180
#define BUTTON_CALIB_START_W 100
#define BUTTON_CALIB_START_H 40

#define BUTTON_CALIB_NEXT_X 110
#define BUTTON_CALIB_NEXT_Y 180
#define BUTTON_CALIB_NEXT_W 100
#define BUTTON_CALIB_NEXT_H 40

#define BUTTON_CALIB_RECORD_X 110
#define BUTTON_CALIB_RECORD_Y 180
#define BUTTON_CALIB_RECORD_W 100
#define BUTTON_CALIB_RECORD_H 40

#define BUTTON_CALIB_APPLY_X 110
#define BUTTON_CALIB_APPLY_Y 180
#define BUTTON_CALIB_APPLY_W 100
#define BUTTON_CALIB_APPLY_H 40

#define BUTTON_CALIB_EXIT_X 110
#define BUTTON_CALIB_EXIT_Y 220
#define BUTTON_CALIB_EXIT_W 100
#define BUTTON_CALIB_EXIT_H 25

// Function prototypes
void initESPNow();
void initDisplay();
void initTouch();
void initSDCard();
void showSDErrorIfFailed();
void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
void onCalibrationResponse(const uint8_t *mac, const uint8_t *incomingData, int len);
void drawLiveView();
void drawGraphView();
void drawCalibrationView();
void checkTouch();
void addDataPoint(payload_t data);
void drawGrid();
void drawGraph();
void sendCalibrationCommand(uint8_t command, float target_sg);
void applyCalibration();
void logDataToSD(payload_t data);
void createNewFermentationFile();
void continueFermentationFile();
void drawRebootPrompt();
void checkExistingFermentation();
void checkOGStability(float currentSG);
void logOGToSD();
float calculateABV(float og, float currentSG);
uint8_t calculateBatteryPercentage(float voltage);

// UI Design System Helper Functions
void drawHeader(const char* title);
void drawFooter(const char* status);
void drawCard(int x, int y, int w, int h);
void drawButton(int x, int y, int w, int h, const char* label, bool active);
void toggleTheme();
void drawBootScreen();
void drawLogo2Optimized(int16_t x, int16_t y);

void setup() {
  Serial.begin(115200);
  DEBUG_INFO_PRINTLN("=== Fermentation Base Station Starting ===");
  
  // Initialize WiFi and display MAC address for ESP-NOW configuration
  WiFi.mode(WIFI_STA);
  delay(100); // Allow WiFi to initialize
  DEBUG_INFO_PRINT("Base Station MAC Address: ");
  DEBUG_INFO_PRINTLN(WiFi.macAddress());
  
  initDisplay();
  
  // Show boot screen immediately after display initialization
  drawBootScreen();
  
  initTouch();
  initSDCard();
  initESPNow();
}

void loop() {
  // Don't process anything during boot screen
  if (!bootScreenComplete) {
    delay(10);
    return;
  }
  
  // Check for touch input
  if (millis() - lastTouchCheck > TOUCH_CHECK_INTERVAL) {
    checkTouch();
    lastTouchCheck = millis();
  }
  
  // Show SD card error message if initialization failed (after boot screen)
  showSDErrorIfFailed();
  
  // Check for existing fermentation after boot screen completes
  if (!fermentationCheckDone) {
    checkExistingFermentation();
    fermentationCheckDone = true;
    
    if (!waitingForRebootChoice) {
      DEBUG_INFO_PRINTLN("Base station ready");
      // Initial screen draw
      drawLiveView();
    }
  }
  
  // Update display only when screen is dirty
  if (screenDirty) {
    switch (currentMode) {
      case LIVE_VIEW:
        drawLiveView();
        break;
      case GRAPH_VIEW:
        drawGraphView();
        break;
      case CALIBRATION_VIEW:
        drawCalibrationView();
        break;
    }
    screenDirty = false;
  }
  
  delay(10);
}

void initESPNow() {
  // ESP-NOW will automatically use the current WiFi channel
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    DEBUG_ERROR_PRINTLN("Error initializing ESP-NOW");
    return;
  }
  
  // Register receive callback (new ESP-IDF signature)
  esp_now_register_recv_cb([](const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    onDataReceived(recv_info->src_addr, data, len);
  });
  
  DEBUG_INFO_PRINTLN("ESP-NOW initialized");
}

void initDisplay() {
  tft.init();
  tft.setRotation(3); // Landscape mode
  // Don't fill screen here - boot screen will handle it
  tft.setTextColor(currentTheme->textPrimary, currentTheme->background);
  
  DEBUG_INFO_PRINTLN("Display initialized");
}

void initTouch() {
  ts.begin();
  ts.setRotation(3);
  
  DEBUG_INFO_PRINTLN("Touch screen initialized");
}

void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(payload_t)) {
    payload_t receivedData;
    memcpy(&receivedData, incomingData, len);
    
    // Track float MAC address for unicast communication
    memcpy(lastFloatMac, mac, 6);
    
    // Register float as peer if not already registered
    if (!floatPeerRegistered) {
      esp_now_peer_info_t peerInfo;
      memcpy(peerInfo.peer_addr, lastFloatMac, 6);
      peerInfo.channel = ESPNOW_CHANNEL;
      peerInfo.encrypt = false;
      
      if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        floatPeerRegistered = true;
        DEBUG_INFO_PRINTLN("Float unit registered as peer");
      } else {
        DEBUG_ERROR_PRINTLN("Failed to register float unit as peer");
      }
    }
    
    // Verify CRC (exclude version field for backward compatibility)
    size_t payload_size = sizeof(receivedData) - sizeof(receivedData.crc) - sizeof(receivedData.version);
    uint16_t calculated_crc = 0xFFFF;
    for (size_t i = 0; i < payload_size; i++) {
      calculated_crc ^= ((uint8_t*)&receivedData)[i + sizeof(receivedData.version)];
      for (uint8_t j = 0; j < 8; j++) {
        calculated_crc = (calculated_crc >> 1) ^ (calculated_crc & 1 ? 0xA001 : 0);
      }
    }
    
    if (calculated_crc != receivedData.crc) {
      DEBUG_ERROR_PRINTF("CRC mismatch: expected %d, got %d\n", receivedData.crc, calculated_crc);
      return;
    }
    
    // Check protocol version
    if (receivedData.version != 2) {
      DEBUG_INFO_PRINTF("Warning: Unknown protocol version %d\n", receivedData.version);
    }
    
    uint8_t batteryPercent = calculateBatteryPercentage(receivedData.battery_voltage);
    
    DEBUG_VERBOSE_PRINTF("Received: seq=%d, angle=%.2f°, density=%.3f, temp=%.1f°C, battery=%.2fV (%d%%)\n", 
                  receivedData.sequence_id, receivedData.angle, receivedData.density, 
                  receivedData.temperature, receivedData.battery_voltage, batteryPercent);
    
    // Check OG stability if not yet captured
    if (!ogCaptured && fermentationFileOpen) {
      checkOGStability(receivedData.density);
    }
    
    // Calculate ABV if OG is captured
    if (ogCaptured) {
      currentABV = calculateABV(originalGravity, receivedData.density);
    }
    
    // Add data to buffer
    addDataPoint(receivedData);
    
    // Log to SD card
    logDataToSD(receivedData);
    
    // Update display immediately for live data
    if (currentMode == LIVE_VIEW) {
      screenDirty = true;
    }
    
    lastUpdate = millis();
    
    // Clear calibration mode flag when receiving normal data
    calibrationModeActive = false;
    
  } else if (len == sizeof(calib_response_t)) {
    // Handle calibration response from float unit
    onCalibrationResponse(mac, incomingData, len);
    
    // Set calibration mode flag when receiving calibration responses
    calibrationModeActive = true;
    
  } else {
    DEBUG_ERROR_PRINTF("Received invalid data size: %d bytes\n", len);
  }
}

void addDataPoint(payload_t data) {
  dataBuffer[dataIndex] = data;
  dataIndex = (dataIndex + 1) % MAX_DATA_POINTS;
  
  if (dataCount < MAX_DATA_POINTS) {
    dataCount++;
  }
}

void drawLiveViewStatic() {
  // Draw header
  drawHeader("Live");
  
  // Clear content area (below header, above footer)
  tft.fillRect(0, 40, SCREEN_W, SCREEN_H - 70, currentTheme->background);
  
  // Draw calibration mode warning if active
  if (calibrationModeActive) {
    tft.setTextColor(currentTheme->error);
    tft.setTextSize(1);
    tft.setCursor(10, 45);
    tft.println(" CALIBRATION MODE ACTIVE - HIGH BATTERY DRAIN ");
  }
  
  if (dataCount == 0) {
    tft.setTextColor(currentTheme->textMuted);
    tft.setTextSize(2);
    tft.setCursor(80, SCREEN_H / 2 - 10);
    tft.println("Waiting for data...");
    drawFooter("No data received");
    return;
  }
  
  // Draw static card layout
  int cardW = 95;
  int cardH = 55;
  int startX = 10;
  int startY = calibrationModeActive ? 65 : 50;
  int spacing = 10;
  
  // Row 1
  // SG card (top-left)
  drawCard(startX, startY, cardW, cardH);
  tft.setTextColor(currentTheme->accent);
  tft.setTextSize(1);
  tft.setCursor(startX + 10, startY + 8);
  tft.println("SG");
  
  // Temperature card (top-center)
  drawCard(startX + cardW + spacing, startY, cardW, cardH);
  tft.setTextColor(currentTheme->textSecondary);
  tft.setTextSize(1);
  tft.setCursor(startX + cardW + spacing + 10, startY + 8);
  tft.println("Temp");
  
  // Angle card (top-right)
  drawCard(startX + (cardW + spacing) * 2, startY, cardW, cardH);
  tft.setTextColor(currentTheme->textSecondary);
  tft.setTextSize(1);
  tft.setCursor(startX + (cardW + spacing) * 2 + 10, startY + 8);
  tft.println("Angle");
  
  // Row 2
  int row2Y = startY + cardH + spacing;
  
  // Battery card (bottom-left)
  drawCard(startX, row2Y, cardW, cardH);
  tft.setTextColor(currentTheme->textSecondary);
  tft.setTextSize(1);
  tft.setCursor(startX + 10, row2Y + 8);
  tft.println("Battery");
  
  // OG card (bottom-center)
  drawCard(startX + cardW + spacing, row2Y, cardW, cardH);
  tft.setTextColor(currentTheme->textSecondary);
  tft.setTextSize(1);
  tft.setCursor(startX + cardW + spacing + 10, row2Y + 8);
  tft.println("OG");
  
  // ABV card (bottom-right)
  drawCard(startX + (cardW + spacing) * 2, row2Y, cardW, cardH);
  tft.setTextColor(currentTheme->textSecondary);
  tft.setTextSize(1);
  tft.setCursor(startX + (cardW + spacing) * 2 + 10, row2Y + 8);
  tft.println("ABV");
}

void updateLiveViewDynamic() {
  if (dataCount == 0) {
    return;
  }
  
  // Get latest data
  payload_t latest = dataBuffer[(dataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
  uint8_t batteryPercent = calculateBatteryPercentage(latest.battery_voltage);
  
  // Layout constants
  int cardW = 95;
  int cardH = 55;
  int startX = 10;
  int startY = calibrationModeActive ? 65 : 50;
  int spacing = 10;
  
  // Update dynamic values
  
  // SG value
  tft.setTextColor(currentTheme->textPrimary);
  tft.setTextSize(3);
  tft.setCursor(startX + 10, startY + 25);
  tft.printf("%.3f", latest.density);
  
  // Temperature value
  tft.setTextColor(currentTheme->textPrimary);
  tft.setTextSize(3);
  tft.setCursor(startX + cardW + spacing + 10, startY + 25);
  tft.printf("%.1f%cC", latest.temperature, 247);
  
  // Angle value
  tft.setTextColor(currentTheme->textPrimary);
  tft.setTextSize(3);
  tft.setCursor(startX + (cardW + spacing) * 2 + 10, startY + 25);
  tft.printf("%.1f%c", latest.angle, 248);
  
  // Row 2
  int row2Y = startY + cardH + spacing;
  
  // Battery value
  tft.setTextColor(currentTheme->textPrimary);
  tft.setTextSize(3);
  tft.setCursor(startX + 10, row2Y + 25);
  tft.printf("%d%%", batteryPercent);
  
  // OG value
  tft.setTextColor(currentTheme->textPrimary);
  tft.setTextSize(3);
  tft.setCursor(startX + cardW + spacing + 10, row2Y + 25);
  if (ogCaptured) {
    tft.printf("%.3f", originalGravity);
  } else {
    tft.setTextColor(currentTheme->textMuted);
    tft.setTextSize(2);
    tft.setCursor(startX + cardW + spacing + 15, row2Y + 28);
    tft.println("...");
  }
  
  // ABV value
  tft.setTextColor(currentTheme->textPrimary);
  tft.setTextSize(3);
  tft.setCursor(startX + (cardW + spacing) * 2 + 10, row2Y + 25);
  if (ogCaptured) {
    tft.printf("%.1f%%", currentABV);
  } else {
    tft.setTextColor(currentTheme->textMuted);
    tft.setTextSize(2);
    tft.setCursor(startX + (cardW + spacing) * 2 + 15, row2Y + 28);
    tft.println("...");
  }
  
  // Update footer with status
  char status[64];
  sprintf(status, "Seq: %d | Batt: %.2fV | %d points", latest.sequence_id, latest.battery_voltage, dataCount);
  drawFooter(status);
}

void drawLiveView() {
  if (!staticElementsDrawn || currentMode != LIVE_VIEW) {
    drawLiveViewStatic();
    staticElementsDrawn = true;
  }
  updateLiveViewDynamic();
}

void drawGraphView() {
  // Draw header
  drawHeader("Graph");
  
  // Clear content area (below header, above footer)
  tft.fillRect(0, 40, SCREEN_W, SCREEN_H - 70, currentTheme->background);
  
  if (dataCount < 2) {
    tft.setTextColor(currentTheme->textMuted);
    tft.setTextSize(2);
    tft.setCursor(50, SCREEN_H / 2 - 10);
    tft.println("Need more data for graph");
    drawFooter("Waiting for data...");
    return;
  }
  
  drawGrid();
  drawGraph();
  
  // Draw footer with status
  char status[64];
  sprintf(status, "Data points: %d", dataCount);
  drawFooter(status);
}

void drawGrid() {
  // Graph area (adjusted for header/footer)
  int graphX = 40;
  int graphY = 50;
  int graphW = SCREEN_W - 60;
  int graphH = SCREEN_H - 90;
  
  // Draw axes
  tft.drawFastHLine(graphX, graphY + graphH, graphW, currentTheme->textPrimary);
  tft.drawFastVLine(graphX, graphY, graphH, currentTheme->textPrimary);
  
  // Draw grid lines
  tft.setTextColor(currentTheme->textMuted);
  tft.setTextSize(1);
  
  // Y-axis labels (SG values from 0.9 to 1.2)
  for (int i = 0; i <= 6; i++) {
    int y = graphY + graphH - (i * graphH / 6);
    tft.drawFastHLine(graphX, y, graphW, currentTheme->gridLine);
    
    float sg = 0.9 + (i * 0.05);
    tft.setCursor(5, y - 3);
    tft.printf("%.2f", sg);
  }
  
  // X-axis labels (time markers)
  for (int i = 0; i <= 5; i++) {
    int x = graphX + (i * graphW / 5);
    tft.drawFastVLine(x, graphY, graphH, currentTheme->gridLine);
    
    tft.setCursor(x - 10, graphY + graphH + 2);
    tft.printf("%d", i * 20);
  }
  
  // Axis labels
  tft.setTextColor(currentTheme->textSecondary);
  tft.setCursor(graphX + graphW/2 - 20, graphY + graphH + 15);
  tft.println("Time (min)");
  
  tft.setCursor(5, graphY + graphH/2 - 20);
  tft.println("Specific Gravity");
}

void drawGraph() {
  if (dataCount < 2) return;
  
  int graphX = 40;
  int graphY = 50;
  int graphW = SCREEN_W - 60;
  int graphH = SCREEN_H - 90;
  
  // Find min/max density for scaling
  float minDensity = 1.0, maxDensity = 1.1;
  
  for (int i = 0; i < dataCount; i++) {
    payload_t data = dataBuffer[(dataIndex - dataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    if (data.density < minDensity) minDensity = data.density;
    if (data.density > maxDensity) maxDensity = data.density;
  }
  
  // Add some padding
  minDensity -= 0.005;
  maxDensity += 0.005;

  // Prevent division by zero
  float densityRange = maxDensity - minDensity;
  if (densityRange == 0) densityRange = 0.001;

  // Draw density line
  int prevX = -1, prevY = -1;

  for (int i = 0; i < dataCount; i++) {
    payload_t data = dataBuffer[(dataIndex - dataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS];

    int x = graphX + (i * graphW / (MAX_DATA_POINTS - 1));
    int y = graphY + graphH - ((data.density - minDensity) / densityRange * graphH);

    if (prevX >= 0 && prevY >= 0) {
      tft.drawLine(prevX, prevY, x, y, currentTheme->accent);
    }

    tft.drawPixel(x, y, currentTheme->primary);
    
    prevX = x;
    prevY = y;
  }
  
  // Draw current value indicator
  if (dataCount > 0) {
    payload_t latest = dataBuffer[(dataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    
    int currentX = graphX + ((dataCount - 1) * graphW / (MAX_DATA_POINTS - 1));
    int currentY = graphY + graphH - ((latest.density - minDensity) / densityRange * graphH);
    
    tft.fillCircle(currentX, currentY, 3, currentTheme->success);
    
    // Show current value in a card
    tft.fillRect(currentX + 10, currentY - 20, 60, 25, currentTheme->cardBackground);
    tft.drawRect(currentX + 10, currentY - 20, 60, 25, currentTheme->border);
    tft.setTextColor(currentTheme->textPrimary);
    tft.setTextSize(1);
    tft.setCursor(currentX + 15, currentY - 12);
    tft.printf("%.3f", latest.density);
  }
}

void drawCalibrationView() {
  // Draw header
  drawHeader("Calib");
  
  // Clear content area (below header, above footer)
  tft.fillRect(0, 40, SCREEN_W, SCREEN_H - 70, currentTheme->background);
  
  // Draw calibration mode warning
  tft.setTextColor(currentTheme->error);
  tft.setTextSize(1);
  tft.setCursor(10, 45);
  tft.println(" HIGH BATTERY DRAIN - PHYSICAL SWITCH ACTIVATED ");
  
  // Draw content based on calibration state
  tft.setTextColor(currentTheme->textPrimary);
  tft.setTextSize(1);
  
  if (calibMode == CALIB_IDLE) {
    drawCard(10, 70, 280, 80);
    tft.setTextColor(currentTheme->textSecondary);
    tft.setCursor(20, 80);
    tft.println("Press START to begin calibration");
    
    drawButton(BUTTON_CALIB_START_X, BUTTON_CALIB_START_Y, BUTTON_CALIB_START_W, BUTTON_CALIB_START_H, "START", false);
    
  } else if (calibMode == CALIB_INSTRUCTIONS) {
    drawCard(10, 70, 280, 100);
    tft.setTextColor(currentTheme->accent);
    tft.setCursor(20, 80);
    tft.println("CALIBRATION INSTRUCTIONS:");
    tft.setTextColor(currentTheme->textPrimary);
    tft.setCursor(20, 95);
    tft.println("1. Get a bowl (min 3L capacity)");
    tft.setCursor(20, 110);
    tft.println("2. Fill with 2L tap water");
    tft.setCursor(20, 125);
    tft.println("3. Place float in water");
    tft.setCursor(20, 140);
    tft.println("4. Wait for float to stabilize");
    
    drawButton(BUTTON_CALIB_NEXT_X, BUTTON_CALIB_NEXT_Y, BUTTON_CALIB_NEXT_W, BUTTON_CALIB_NEXT_H, "NEXT", false);
    
  } else if (calibMode == CALIB_POINT1) {
    drawCard(10, 70, 280, 90);
    tft.setTextColor(currentTheme->accent);
    tft.setCursor(20, 80);
    tft.println("STEP 1/4 - WATER (SG=1.000)");
    tft.setTextColor(currentTheme->textPrimary);
    tft.setCursor(20, 95);
    tft.println("Float should be in 2L tap water");
    tft.setCursor(20, 110);
    tft.println("Wait for float to stabilize");
    tft.setCursor(20, 125);
    tft.println("Press RECORD when stable");
    
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "RECORD", calibAngles[0] > 0);
    
  } else if (calibMode == CALIB_POINT2) {
    drawCard(10, 70, 280, 90);
    tft.setTextColor(currentTheme->accent);
    tft.setCursor(20, 80);
    tft.println("STEP 2/4 - ADD SUGAR (SG=1.040)");
    tft.setTextColor(currentTheme->textPrimary);
    tft.setCursor(20, 95);
    tft.printf("Add %dg normal sugar to bowl", sugarAmounts[1]);
    tft.setCursor(20, 110);
    tft.println("Swirl to dissolve completely");
    tft.setCursor(20, 125);
    tft.println("Wait for float to stabilize");
    tft.setCursor(20, 140);
    tft.println("Press RECORD when stable");
    
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "RECORD", calibAngles[1] > 0);
    
  } else if (calibMode == CALIB_POINT3) {
    drawCard(10, 70, 280, 90);
    tft.setTextColor(currentTheme->accent);
    tft.setCursor(20, 80);
    tft.println("STEP 3/4 - ADD SUGAR (SG=1.080)");
    tft.setTextColor(currentTheme->textPrimary);
    tft.setCursor(20, 95);
    tft.printf("Add %dg normal sugar to bowl", sugarAmounts[2]);
    tft.setCursor(20, 110);
    tft.println("Swirl to dissolve completely");
    tft.setCursor(20, 125);
    tft.println("Wait for float to stabilize");
    tft.setCursor(20, 140);
    tft.println("Press RECORD when stable");
    
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "RECORD", calibAngles[2] > 0);
    
  } else if (calibMode == CALIB_POINT4) {
    drawCard(10, 70, 280, 90);
    tft.setTextColor(currentTheme->accent);
    tft.setCursor(20, 80);
    tft.println("STEP 4/4 - ADD SUGAR (SG=1.120)");
    tft.setTextColor(currentTheme->textPrimary);
    tft.setCursor(20, 95);
    tft.printf("Add %dg normal sugar to bowl", sugarAmounts[3]);
    tft.setCursor(20, 110);
    tft.println("Swirl to dissolve completely");
    tft.setCursor(20, 125);
    tft.println("Wait for float to stabilize");
    tft.setCursor(20, 140);
    tft.println("Press RECORD when stable");
    
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "RECORD", calibAngles[3] > 0);
    
  } else if (calibMode == CALIB_COMPLETE) {
    drawCard(10, 70, 280, 90);
    tft.setTextColor(currentTheme->textPrimary);
    tft.setCursor(20, 80);
    tft.println("All 4 points recorded!");
    tft.setCursor(20, 95);
    tft.println("Press APPLY to calculate");
    tft.setCursor(20, 110);
    tft.println("3rd degree polynomial");
    
    // Show recorded angles
    tft.setTextColor(currentTheme->accent);
    tft.setCursor(20, 130);
    tft.printf("P1: %.1f%c  P2: %.1f%c", calibAngles[0], 248, calibAngles[1], 248);
    tft.setCursor(20, 145);
    tft.printf("P3: %.1f%c  P4: %.1f%c", calibAngles[2], 248, calibAngles[3], 248);
    
    drawButton(BUTTON_CALIB_APPLY_X, BUTTON_CALIB_APPLY_Y, BUTTON_CALIB_APPLY_W, BUTTON_CALIB_APPLY_H, "APPLY", false);
  }
  
  // Exit button (always shown)
  drawButton(BUTTON_CALIB_EXIT_X, BUTTON_CALIB_EXIT_Y, BUTTON_CALIB_EXIT_W, BUTTON_CALIB_EXIT_H, "EXIT", false);
  
  // Draw footer
  drawFooter("Calibration Mode");
}

// UI Design System Helper Functions Implementation

void drawHeader(const char* title) {
  // Draw header background
  tft.fillRect(0, 0, SCREEN_W, 40, currentTheme->primary);
  
  // Draw SGNode branding
  tft.setTextColor(currentTheme->primaryText);
  tft.setTextSize(2);
  tft.setCursor(10, 12);
  tft.println("SGNode");
  
  // Draw navigation buttons on the right
  int buttonWidth = 60;
  int buttonHeight = 30;
  int buttonY = 5;
  int spacing = 5;
  
  // Calculate button positions from right side
  int calibX = SCREEN_W - buttonWidth - 10;
  int graphX = calibX - buttonWidth - spacing;
  int liveX = graphX - buttonWidth - spacing;
  
  // LIVE button
  drawButton(liveX, buttonY, buttonWidth, buttonHeight, "LIVE", currentMode == LIVE_VIEW);
  
  // GRAPH button
  drawButton(graphX, buttonY, buttonWidth, buttonHeight, "GRAPH", currentMode == GRAPH_VIEW);
  
  // CALIB button
  drawButton(calibX, buttonY, buttonWidth, buttonHeight, "CALIB", currentMode == CALIBRATION_VIEW);
}

void drawFooter(const char* status) {
  // Draw footer background
  tft.fillRect(0, SCREEN_H - 30, SCREEN_W, 30, currentTheme->cardBackground);
  
  // Draw status text
  tft.setTextColor(currentTheme->textSecondary);
  tft.setTextSize(1);
  tft.setCursor(10, SCREEN_H - 20);
  tft.println(status);
}

void drawCard(int x, int y, int w, int h) {
  // Draw card background with rounded corners simulation
  tft.fillRect(x + 2, y + 2, w - 4, h - 4, currentTheme->cardBackground);
  tft.drawRect(x, y, w, h, currentTheme->border);
}

void drawButton(int x, int y, int w, int h, const char* label, bool active) {
  if (active) {
    tft.fillRect(x, y, w, h, currentTheme->primary);
    tft.setTextColor(currentTheme->primaryText);
  } else {
    tft.fillRect(x, y, w, h, currentTheme->buttonInactive);
    tft.setTextColor(currentTheme->textPrimary);
  }
  tft.drawRect(x, y, w, h, currentTheme->border);
  
  // Center text - approximate centering without getTextBounds
  tft.setTextSize(1);
  int textLen = strlen(label);
  int16_t w1 = textLen * 6; // Approximate width (6 pixels per character)
  int16_t h1 = 8; // Approximate height for text size 1
  tft.setCursor(x + (w - w1) / 2, y + (h - h1) / 2);
  tft.print(label); // Changed from println to print
}

void toggleTheme() {
  darkMode = !darkMode;
  currentTheme = darkMode ? &darkTheme : &lightTheme;
  
  // Redraw screen with new theme
  screenDirty = true;
}

void drawBootScreen() {
  tft.fillScreen(currentTheme->background);
  
  // Center logo on screen
  int logoX = (SCREEN_W - logo2_width) / 2;
  int logoY = (SCREEN_H - logo2_height) / 2 - 20; // Move up to make room for text
  
  // Draw optimized logo (only non-white pixels)
  drawLogo2Optimized(logoX, logoY);
  
  delay(2000); // Show boot screen for 2 seconds
  
  // Mark boot screen as complete and record time
  bootScreenComplete = true;
  bootScreenCompleteTime = millis();
}

void drawLogo2Optimized(int16_t x, int16_t y) {
  // Draw each non-white pixel at the specified offset
  for (uint16_t i = 0; i < logo2_pixel_count; i++) {
    Pixel pixel = logo2_pixels[i];
    // Draw pixel at screen coordinates (x + pixel.x, y + pixel.y)
    tft.drawPixel(x + pixel.x, y + pixel.y, pixel.color);
  }
}

void checkTouch() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    // ... (rest of the code remains the same)
    touchX = map(p.x, 200, 3700, 0, SCREEN_W);
    touchY = map(p.y, 240, 3800, 0, SCREEN_H);
    // Handle reboot prompt buttons
    if (waitingForRebootChoice) {
      if (touchX >= BUTTON_CONTINUE_X && touchX <= BUTTON_CONTINUE_X + BUTTON_CONTINUE_W &&
          touchY >= BUTTON_CONTINUE_Y && touchY <= BUTTON_CONTINUE_Y + BUTTON_CONTINUE_H) {
        continueFermentationFile();
        waitingForRebootChoice = false;
        screenDirty = true;
        DEBUG_INFO_PRINTLN("Continuing existing fermentation");
      }
      
      if (touchX >= BUTTON_NEW_X && touchX <= BUTTON_NEW_X + BUTTON_NEW_W &&
          touchY >= BUTTON_NEW_Y && touchY <= BUTTON_NEW_Y + BUTTON_NEW_H) {
        createNewFermentationFile();
        waitingForRebootChoice = false;
        screenDirty = true;
        DEBUG_INFO_PRINTLN("Starting new fermentation");
      }
      
      delay(50); // Debounce
      return;
    }
    
    // Check header navigation button presses
    int buttonWidth = 60;
    int buttonHeight = 30;
    int buttonY = 5;
    int spacing = 5;
    int calibX = SCREEN_W - buttonWidth - 10;
    int graphX = calibX - buttonWidth - spacing;
    int liveX = graphX - buttonWidth - spacing;
    
    // LIVE button
    if (touchX >= liveX && touchX <= liveX + buttonWidth &&
        touchY >= buttonY && touchY <= buttonY + buttonHeight) {
      if (currentMode != LIVE_VIEW) {
        currentMode = LIVE_VIEW;
        screenDirty = true;
        DEBUG_INFO_PRINTLN("Switched to Live View");
      }
    }
    
    // GRAPH button
    if (touchX >= graphX && touchX <= graphX + buttonWidth &&
        touchY >= buttonY && touchY <= buttonY + buttonHeight) {
      if (currentMode != GRAPH_VIEW) {
        currentMode = GRAPH_VIEW;
        screenDirty = true;
        DEBUG_INFO_PRINTLN("Switched to Graph View");
      }
    }
    
    // CALIB button
    if (touchX >= calibX && touchX <= calibX + buttonWidth &&
        touchY >= buttonY && touchY <= buttonY + buttonHeight) {
      if (currentMode != CALIBRATION_VIEW) {
        currentMode = CALIBRATION_VIEW;
        screenDirty = true;
        DEBUG_INFO_PRINTLN("Switched to Calibration View");
      }
    }
    
    // Handle calibration screen buttons
    if (currentMode == CALIBRATION_VIEW) {
      // Start button
      if (calibMode == CALIB_IDLE &&
          touchX >= BUTTON_CALIB_START_X && touchX <= BUTTON_CALIB_START_X + BUTTON_CALIB_START_W &&
          touchY >= BUTTON_CALIB_START_Y && touchY <= BUTTON_CALIB_START_Y + BUTTON_CALIB_START_H) {
        calibMode = CALIB_INSTRUCTIONS;
        screenDirty = true;
        DEBUG_INFO_PRINTLN("Started calibration wizard");
      }
      
      // Next button (instructions to step 1)
      if (calibMode == CALIB_INSTRUCTIONS &&
          touchX >= BUTTON_CALIB_NEXT_X && touchX <= BUTTON_CALIB_NEXT_X + BUTTON_CALIB_NEXT_W &&
          touchY >= BUTTON_CALIB_NEXT_Y && touchY <= BUTTON_CALIB_NEXT_Y + BUTTON_CALIB_NEXT_H) {
        calibMode = CALIB_POINT1;
        screenDirty = true;
        DEBUG_INFO_PRINTLN("Moved to Step 1");
      }
      
      // Record button for each step
      if (calibMode == CALIB_POINT1 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(0, 1.000);
        DEBUG_INFO_PRINTF("Sent calibration command for Point 1 (SG=1.000)\n");
      }
      
      if (calibMode == CALIB_POINT2 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(1, 1.040);
        DEBUG_INFO_PRINTF("Sent calibration command for Point 2 (SG=1.040)\n");
      }
      
      if (calibMode == CALIB_POINT3 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(2, 1.080);
        DEBUG_INFO_PRINTF("Sent calibration command for Point 3 (SG=1.080)\n");
      }
      
      if (calibMode == CALIB_POINT4 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(3, 1.120);
        DEBUG_INFO_PRINTF("Sent calibration command for Point 4 (SG=1.120)\n");
      }
      
      // Apply button
      if (calibMode == CALIB_COMPLETE &&
          touchX >= BUTTON_CALIB_APPLY_X && touchX <= BUTTON_CALIB_APPLY_X + BUTTON_CALIB_APPLY_W &&
          touchY >= BUTTON_CALIB_APPLY_Y && touchY <= BUTTON_CALIB_APPLY_Y + BUTTON_CALIB_APPLY_H) {
        applyCalibration();
        screenDirty = true;
        DEBUG_INFO_PRINTLN("Applied calibration");
      }
      
      // Exit button
      if (touchX >= BUTTON_CALIB_EXIT_X && touchX <= BUTTON_CALIB_EXIT_X + BUTTON_CALIB_EXIT_W &&
          touchY >= BUTTON_CALIB_EXIT_Y && touchY <= BUTTON_CALIB_EXIT_Y + BUTTON_CALIB_EXIT_H) {
        currentMode = LIVE_VIEW;
        calibMode = CALIB_IDLE;
        // Reset calibration data
        for (int i = 0; i < 4; i++) calibAngles[i] = 0.0;
        screenDirty = true;
        DEBUG_INFO_PRINTLN("Exited calibration mode");
      }
    }
    
    delay(50); // Debounce
  }
}

void sendCalibrationCommand(uint8_t command, float target_sg) {
  // Send calibration command to float unit using unicast
  if (!floatPeerRegistered) {
    DEBUG_ERROR_PRINTLN("Cannot send calibration command - float unit not registered");
    return;
  }
  
  calib_command_t calibCmd;
  calibCmd.command = command;
  calibCmd.target_sg = target_sg;
  calibCmd.request_id = currentRequestId++;
  
  esp_err_t result = esp_now_send(lastFloatMac, (uint8_t*)&calibCmd, sizeof(calibCmd));
  
  if (result == ESP_OK) {
    DEBUG_INFO_PRINTF("Calibration command sent: cmd=%d, target_sg=%.3f, request_id=%d\n", 
                      command, target_sg, currentRequestId - 1);
  } else {
    DEBUG_ERROR_PRINTF("Failed to send calibration command: %d\n", result);
  }
}

void applyCalibration() {
  // Apply calibration - send command to calculate polynomial coefficients
  if (!floatPeerRegistered) {
    DEBUG_ERROR_PRINTLN("Cannot apply calibration - float unit not registered");
    return;
  }
  
  calib_command_t calibCmd;
  calibCmd.command = 4; // APPLY_CALIBRATION
  calibCmd.target_sg = 0.0;
  calibCmd.request_id = currentRequestId++;
  
  esp_err_t result = esp_now_send(lastFloatMac, (uint8_t*)&calibCmd, sizeof(calibCmd));
  
  if (result == ESP_OK) {
    DEBUG_INFO_PRINTF("Apply calibration command sent: request_id=%d\n", currentRequestId - 1);
  } else {
    DEBUG_ERROR_PRINTF("Failed to send apply calibration command: %d\n", result);
  }
}

void onCalibrationResponse(const uint8_t *mac, const uint8_t *incomingData, int len) {
  // Handle calibration response from float unit
  calib_response_t calibResp;
  memcpy(&calibResp, incomingData, len);
  
  DEBUG_VERBOSE_PRINTF("Calibration response: angle=%.2f°, sg=%.3f, request_id=%d\n", 
                calibResp.angle, calibResp.sg, calibResp.request_id);
  
  if (calibResp.response_type == 0) {
    // Calibration data - store angle based on which point was calibrated
    if (calibResp.request_id == currentRequestId - 1) {
      // Determine which calibration point this response is for based on target_sg
      if (calibResp.sg >= 0.995 && calibResp.sg <= 1.005) {
        // Point 1 - Water (SG=1.000)
        calibAngles[0] = calibResp.angle;
        DEBUG_VERBOSE_PRINTF("Stored Point 1 angle: %.2f°\n", calibAngles[0]);
        calibMode = CALIB_POINT2;
        screenDirty = true;
      } else if (calibResp.sg >= 1.035 && calibResp.sg <= 1.045) {
        // Point 2 - Light Sugar (SG=1.040)
        calibAngles[1] = calibResp.angle;
        DEBUG_VERBOSE_PRINTF("Stored Point 2 angle: %.2f°\n", calibAngles[1]);
        calibMode = CALIB_POINT3;
        screenDirty = true;
      } else if (calibResp.sg >= 1.075 && calibResp.sg <= 1.085) {
        // Point 3 - Medium Sugar (SG=1.080)
        calibAngles[2] = calibResp.angle;
        DEBUG_VERBOSE_PRINTF("Stored Point 3 angle: %.2f°\n", calibAngles[2]);
        calibMode = CALIB_POINT4;
        screenDirty = true;
      } else if (calibResp.sg >= 1.115 && calibResp.sg <= 1.125) {
        // Point 4 - Heavy Sugar (SG=1.120)
        calibAngles[3] = calibResp.angle;
        DEBUG_VERBOSE_PRINTF("Stored Point 4 angle: %.2f°\n", calibAngles[3]);
        calibMode = CALIB_COMPLETE;
        screenDirty = true;
      }
    }
  }
}

void initSDCard() {
  DEBUG_INFO_PRINTLN("Initializing SD card...");
  
  if (!SD.begin(SD_CS)) {
    DEBUG_ERROR_PRINTLN("SD card initialization failed!");
    sdCardInitialized = false;
    return;
  }
  
  DEBUG_INFO_PRINTLN("SD card initialized successfully");
  sdCardInitialized = true;
  
  // Check if fermentation directory exists
  if (!SD.exists("/fermentation")) {
    SD.mkdir("/fermentation");
    DEBUG_INFO_PRINTLN("Created fermentation directory");
  }
}

void showSDErrorIfFailed() {
  // Show error if SD card failed to initialize, boot screen is complete, and 2-second delay has passed
  if (!sdCardInitialized && bootScreenComplete && !sdErrorMessageShown) {
    // Check if 2-second delay has passed since boot screen completed
    if (millis() - bootScreenCompleteTime >= SD_ERROR_DELAY) {
      // Draw error message in lower right corner without background
      tft.setTextColor(currentTheme->error);
      tft.setTextSize(1);
      tft.setCursor(SCREEN_W - 150, SCREEN_H - 20);
      tft.println("SD Card Failed!");
      
      // Mark error message as shown
      sdErrorMessageShown = true;
    }
  }
}

void checkExistingFermentation() {
  if (!SD.begin(SD_CS)) {
    DEBUG_ERROR_PRINTLN("SD card not available, skipping fermentation check");
    return;
  }
  
  // Check for existing fermentation files
  File root = SD.open("/fermentation");
  if (!root) {
    DEBUG_ERROR_PRINTLN("Failed to open fermentation directory");
    return;
  }
  
  File file = root.openNextFile();
  bool hasExistingFiles = false;
  
  while (file) {
    if (!file.isDirectory()) {
      hasExistingFiles = true;
      // Get the most recent file
      strcpy(currentFermentationFile, file.name());
      DEBUG_INFO_PRINTF("Found existing fermentation file: %s\n", currentFermentationFile);
      break;
    }
    file = root.openNextFile();
  }
  
  root.close();
  
  if (hasExistingFiles) {
    waitingForRebootChoice = true;
    drawRebootPrompt();
  } else {
    // No existing files, create new one
    createNewFermentationFile();
  }
}

void drawRebootPrompt() {
  tft.fillScreen(currentTheme->background);
  tft.setTextColor(currentTheme->textPrimary);
  
  // Title
  tft.setTextSize(2);
  tft.setTextColor(currentTheme->primary);
  tft.setCursor(40, 30);
  tft.println("Fermentation");
  tft.setCursor(60, 55);
  tft.println("Found!");
  
  // Question
  tft.setTextSize(1);
  tft.setTextColor(currentTheme->textPrimary);
  tft.setCursor(20, 90);
  tft.println("Continue existing");
  tft.setCursor(50, 105);
  tft.println("or start new?");
  
  // Continue button
  drawButton(BUTTON_CONTINUE_X, BUTTON_CONTINUE_Y, BUTTON_CONTINUE_W, BUTTON_CONTINUE_H, "CONTINUE", false);
  
  // New button
  drawButton(BUTTON_NEW_X, BUTTON_NEW_Y, BUTTON_NEW_W, BUTTON_NEW_H, "NEW", false);
  
  // File name
  tft.setTextColor(currentTheme->accent);
  tft.setCursor(40, 190);
  tft.print("File: ");
  tft.print(currentFermentationFile);
}

void createNewFermentationFile() {
  // Generate filename with timestamp
  char filename[32];
  
  // Get current time from RTC or millis
  unsigned long currentTime = millis();
  int seconds = (currentTime / 1000) % 60;
  int minutes = (currentTime / 60000) % 60;
  int hours = (currentTime / 3600000) % 24;
  int days = (currentTime / 86400000);
  
  sprintf(filename, "/fermentation/ferm_%02dd%02dh%02dm%02ds.csv", days, hours, minutes, seconds);
  
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    DEBUG_ERROR_PRINTLN("Failed to create new fermentation file");
    return;
  }
  
  // Write CSV header
  file.println("uptime_s,angle,density,temperature,battery_voltage,battery_percent");
  file.flush();  // Ensure header is written to SD card
  file.close();
  
  strcpy(currentFermentationFile, filename);
  fermentationFileOpen = true;
  
  DEBUG_INFO_PRINTF("Created new fermentation file: %s\n", currentFermentationFile);
}

void continueFermentationFile() {
  fermentationFileOpen = true;
  DEBUG_INFO_PRINTF("Continuing fermentation file: %s\n", currentFermentationFile);
}

void logDataToSD(payload_t data) {
  if (!fermentationFileOpen || strlen(currentFermentationFile) == 0) {
    return;
  }
  
  File file = SD.open(currentFermentationFile, FILE_APPEND);
  if (!file) {
    DEBUG_ERROR_PRINTLN("Failed to open fermentation file for writing");
    return;
  }
  
  // Write CSV data
  uint8_t batteryPercent = calculateBatteryPercentage(data.battery_voltage);
  int bytesWritten = file.printf("%lu,%.2f,%.4f,%.2f,%.2f,%d\n", 
             data.uptime_s, data.angle, data.density, data.temperature, 
             data.battery_voltage, batteryPercent);
  
  file.flush();  // Ensure data is written to SD card
  file.close();
  
  if (bytesWritten > 0) {
    DEBUG_VERBOSE_PRINTF("Logged data to SD: SG=%.4f, Temp=%.2f°C, Battery=%.2fV (%d%%)\n", 
                 data.density, data.temperature, data.battery_voltage, batteryPercent);
  } else {
    DEBUG_ERROR_PRINTLN("Failed to write data to SD card");
  }
}

void checkOGStability(float currentSG) {
  // Store current reading in the last 3 readings buffer
  last3Readings[readingCount % 3] = currentSG;
  readingCount++;
  
  DEBUG_VERBOSE_PRINTF("OG check: reading #%d, SG=%.4f\n", readingCount, currentSG);
  
  // Check if we have at least 3 readings
  if (readingCount >= 3) {
    // Get the last 3 readings
    float r1 = last3Readings[(readingCount - 1) % 3];
    float r2 = last3Readings[(readingCount - 2) % 3];
    float r3 = last3Readings[(readingCount - 3) % 3];
    
    // Calculate variance
    float maxSG = max(r1, max(r2, r3));
    float minSG = min(r1, min(r2, r3));
    float variance = maxSG - minSG;
    
    DEBUG_VERBOSE_PRINTF("Last 3 readings: %.4f, %.4f, %.4f, variance: %.4f\n", r1, r2, r3, variance);
    
    // Check if 3 readings in a row are stable (within threshold)
    if (variance < OG_STABILITY_THRESHOLD) {
      // Calculate average of the 3 stable readings
      originalGravity = (r1 + r2 + r3) / 3.0;
      ogCaptured = true;
      DEBUG_INFO_PRINTF("OG captured as stable: %.4f (average of 3 readings)\n", originalGravity);
      logOGToSD();
      return;
    }
  }
  
  // If we reach OG_MAX_READINGS without stable OG, use average of last 3
  if (readingCount >= OG_MAX_READINGS && !ogCaptured) {
    float r1 = last3Readings[(readingCount - 1) % 3];
    float r2 = last3Readings[(readingCount - 2) % 3];
    float r3 = last3Readings[(readingCount - 3) % 3];
    originalGravity = (r1 + r2 + r3) / 3.0;
    ogCaptured = true;
    DEBUG_INFO_PRINTF("OG captured by fallback (reading #%d): %.4f (average of last 3)\n", readingCount, originalGravity);
    logOGToSD();
  }
}

void logOGToSD() {
  if (!fermentationFileOpen || strlen(currentFermentationFile) == 0) {
    return;
  }
  
  File file = SD.open(currentFermentationFile, FILE_APPEND);
  if (!file) {
    DEBUG_ERROR_PRINTLN("Failed to open fermentation file for OG logging");
    return;
  }
  
  // Write OG as a comment line in the CSV file
  int bytesWritten = file.printf("# OG_CAPTURED: %.4f at reading #%d\n", originalGravity, readingCount);
  
  file.flush();  // Ensure OG is written to SD card
  file.close();
  
  if (bytesWritten > 0) {
    DEBUG_INFO_PRINTF("OG logged to SD: %.4f at reading #%d\n", originalGravity, readingCount);
  } else {
    DEBUG_ERROR_PRINTLN("Failed to write OG to SD card");
  }
}

float calculateABV(float og, float currentSG) {
  // Standard ABV formula: (OG - FG) * 131.25
  float abv = (og - currentSG) * 131.25;
  
  // Ensure ABV is not negative
  if (abv < 0) abv = 0;
  
  return abv;
}

uint8_t calculateBatteryPercentage(float voltage) {
  // Convert voltage to percentage (assuming 18650 Li-ion: 3.0V = 0%, 4.2V = 100%)
  if (voltage <= 3.0) return 0;
  if (voltage >= 4.2) return 100;
  
  // Linear interpolation between 3.0V and 4.2V
  return (uint8_t)((voltage - 3.0) / 1.2 * 100);
}