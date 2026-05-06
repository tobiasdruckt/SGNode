/*
 * Fermentation Monitor - Base Station
 * Receives data via ESP-NOW and displays on TFT with touch interface
 * Shows real-time data and historical graphs
 * 
 * REQUIRED LIBRARIES (install via Arduino Library Manager):
 * - TFT_eSPI (by Bodmer) - includes built-in touch support
 * - SD (built-in)
 * - FS (built-in)
 * - SPI (built-in)
 */

#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <EEPROM.h>
#include "Logo2_Optimized.h"
#include "ui_tokens.h"
#include "ui_components.h"
#include "../SGNode_Shared/sg_protocol.h"

// 4.0inch ESP32-32E Display configuration - ST7796S landscape mode
#define SCREEN_W    480  // Landscape width (320x480 native rotated)
#define SCREEN_H    320  // Landscape height
#define DISPLAY_CS  15   // LCD Chip Select
#define DISPLAY_DC  2    // Data/Command
#define DISPLAY_RST -1   // Reset (uses ESP32 reset)

// Custom color definitions (TFT_eSPI doesn't have TFT_GRAY)
#define TFT_GRAY 0x8410  // Dark gray color

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
  uint16_t gold;  // Gold accent color
  
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
  .gold = rgb565(199, 138, 26),            // Gold accent
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
  .gold = rgb565(199, 138, 26),            // Gold accent
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

// Screen dirty flag for efficient display updates
bool screenDirty = true;
bool staticElementsDrawn = false;

// Region-based dirty flags for LIVE view
bool heroDirty = true;
bool tileTempDirty = true;
bool tileAngleDirty = true;
bool topbarDirty = true;

// Dirty flags for LIVE_DETAILS_VIEW (Battery display there only)
bool tileAbvDirty = true;  // Now used in LIVE_VIEW for ABV tile
bool detailsDirty = true;

// Touch screen uses TFT_eSPI built-in support (shares SPI with display)
// Touch controller is physically wired to display SPI pins (12,13,14)

// Time-based SPI separation approach
// Create separate SPI instance for SD card to avoid TFT/touch conflicts
SPIClass sdSPI(HSPI);

// SD card pin assignment (using standard ESP32 pins to avoid TFT/touch conflicts)
// TFT and touch use pins 12,13,14, so we use different pins for SD
#define SD_CS       5   // SD card chip select
#define SD_MOSI    23   // SD card SPI bus write data signal
#define SD_MISO    19   // SD card SPI bus read data signal  
#define SD_SCK     18   // SD card SPI bus clock signal

// SD card enable/disable flag
#define SD_ENABLED  true

// ESP-NOW configuration
#define ESPNOW_CHANNEL 1

// Data buffering system to avoid real-time SD operations
#define MAX_DATA_BUFFER 500  // Buffer up to 500 sensor readings (increased from 100)
struct BufferedData {
  payload_t data;
  unsigned long timestamp;
  bool written;
};
BufferedData dataBuffer[MAX_DATA_BUFFER];
int bufferIndex = 0;
int bufferedCount = 0;

// SD card management variables
bool sdInitialized = false;
bool sdCardPresent = false;
unsigned long lastSDWrite = 0;
const unsigned long SD_WRITE_INTERVAL = 5000;  // Write to SD every 5 seconds
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_TIMEOUT = 3000;     // 3 seconds of inactivity before SD operations

// Data buffer for historical data (display)
#define MAX_DATA_POINTS 100
payload_t displayDataBuffer[MAX_DATA_POINTS];
int displayDataIndex = 0;
int displayDataCount = 0;

// ABV calculation variables
float originalGravity = 0.0;  // OG captured when stable
float currentABV = 0.0;       // Calculated ABV
int readingCount = 0;         // Number of readings since fermentation started
float last3Readings[3] = {0.0, 0.0, 0.0}; // Last 3 SG readings for stability check
bool ogCaptured = false;      // Whether OG has been captured
#define OG_STABILITY_THRESHOLD 0.005 // SG variance threshold for stability
#define OG_MAX_READINGS 10     // Force capture at this reading if not stable

// Display objects - TFT_eSPI includes built-in XPT2046 touch support
TFT_eSPI tft = TFT_eSPI();

// UI state
enum DisplayMode {
  LIVE_VIEW,
  GRAPH_VIEW,
  CALIBRATION_VIEW,
  MORE_VIEW,
  LIVE_DETAILS_VIEW
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
  CALIB_COMPLETE,
  CALIB_APPLYING,
  CALIB_FAILED
};

CalibMode calibMode = CALIB_IDLE;
float calibAngles[4] = {0.0, 0.0, 0.0, 0.0}; // Store angles for 4 calibration points
float calibSG[4] = {1.000, 1.040, 1.080, 1.120}; // Target SG values for 4 points
int sugarAmounts[4] = {0, 80, 80, 80}; // Sugar to add at each step (grams) - for 2L water
uint8_t currentRequestId = 0;
bool calibrationModeActive = false; // Track if float is in calibration mode

// Wait notification system
unsigned long waitStartTime = 0;
bool waitActive = false;
int waitDuration = 0; // Duration in milliseconds

// Calibration response flood protection
unsigned long lastCalibrationResponse = 0;
const unsigned long CALIBRATION_RESPONSE_RATE_LIMIT = 500; // 500ms minimum between responses (reduced from 1000ms)
int calibrationResponseCount = 0;
const int MAX_CALIBRATION_RESPONSES_PER_MINUTE = 30; // Max 30 responses per minute


// Float unit MAC address storage
uint8_t floatMac[6] = {0}; // Store float unit MAC address
bool floatMacKnown = false; // Flag to indicate if float MAC is known

// EEPROM addresses for float MAC storage
#define EEPROM_FLOAT_MAC_MAGIC 100
#define EEPROM_FLOAT_MAC_ADDR 104
#define FLOAT_MAC_MAGIC 0x4D414346 // "MACF" in hex

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

// Create New Fermentation dialog buttons
#define BUTTON_DIALOG_YES_X 80
#define BUTTON_DIALOG_YES_Y 180
#define BUTTON_DIALOG_YES_W 100
#define BUTTON_DIALOG_YES_H 40

#define BUTTON_DIALOG_BACK_X 220
#define BUTTON_DIALOG_BACK_Y 180
#define BUTTON_DIALOG_BACK_W 100
#define BUTTON_DIALOG_BACK_H 40

// SD card and file management
char currentFermentationFile[64] = "";
bool fermentationFileOpen = false;
bool rebootPromptShown = false;
bool waitingForRebootChoice = false;
bool sdErrorMessageShown = false;
bool showingCreateNewDialog = false;

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
void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
void onCalibrationResponse(const uint8_t *mac, const uint8_t *incomingData, int len);
void onCalibrationCommandFromFloat(const uint8_t *mac, const uint8_t *incomingData, int len);
void drawLiveView();
void drawGraphView();
void drawCalibrationView();
void drawMoreView();
void drawLiveDetailsView();
void checkTouch();
void addDataPoint(payload_t data);
void drawGrid();
void drawGraph();
void loadFloatMacFromEEPROM();
void saveFloatMacToEEPROM();
void startWait(int durationMs);
void checkWaitTimeout();
void stopWait();
void sendCalibrationCommand(uint8_t command, float target_sg);
void applyCalibration();
bool logDataToSD(payload_t data);
void createNewFermentationFile();
void continueFermentationFile();
void drawRebootPrompt();
void drawCreateNewDialog();
void checkExistingFermentation();
bool loadHistoricalDataFromCSV(const char* filename);
void checkOGStability(float currentSG);
void logOGToSD();
float calculateABV(float og, float currentSG);
uint8_t calculateBatteryPercentage(float voltage);
void showSDErrorIfFailed();

// Time-based SPI separation functions
bool mountSDTemporarily();
void dismountSD();
void bufferSensorData(payload_t data);
void writeBufferedDataToSD();
bool isSafeForSDOperation();

// UI Design System Helper Functions
void drawHeader(const char* title);
void drawFooter(const char* status);
void drawCard(int x, int y, int w, int h);
void drawButton(int x, int y, int w, int h, const char* label, bool active);
void toggleTheme();
void drawBootScreen();
void uiInitColors();

void setup() {
  Serial.begin(115200);
  Serial.println("=== Fermentation Base Station Starting ===");
  
  // Initialize UI color tokens from theme
  uiInitColors();
  
  initDisplay();
  initTouch();
  
  // Initialize SD CS pin but don't mount SD yet (to avoid touch interference)
  #if SD_ENABLED
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH); // Ensure SD CS is HIGH initially
    Serial.println("SD card CS pin initialized (not mounted yet)");
    Serial.println("Touch ready - SD will mount temporarily when needed");
  #else
    Serial.println("SD card DISABLED");
  #endif
  
  initESPNow();
  
  // Load float MAC from EEPROM for permanent pairing
  loadFloatMacFromEEPROM();
  
  // Show boot screen first
  drawBootScreen();
  
  // Check for existing fermentation and auto-continue or create new
  checkExistingFermentation();
  
  Serial.println("Base station ready");
  // Initial screen draw
  drawLiveView();
}

void loop() {
  // Check for touch input
  if (millis() - lastTouchCheck > TOUCH_CHECK_INTERVAL) {
    checkTouch();
    lastTouchCheck = millis();
  }
  
  // Check wait timeout for calibration
  checkWaitTimeout();
  
  // Check if it's safe to perform SD operations (no recent touch activity)
  if (isSafeForSDOperation() && (millis() - lastSDWrite) > SD_WRITE_INTERVAL) {
    writeBufferedDataToSD();
    lastSDWrite = millis();
  }
  
  // Update display only when screen is dirty
  if (screenDirty) {
    // Draw dialog on top if active
    if (showingCreateNewDialog) {
      drawCreateNewDialog();
    } else {
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
        case MORE_VIEW:
          drawMoreView();
          break;
        case LIVE_DETAILS_VIEW:
          drawLiveDetailsView();
          break;
      }
    }
    screenDirty = false;
  } else if (displayDataCount > 0) {
    // Update topbar if battery changed for all views
    if (topbarDirty) {
      uint8_t battPercent = 0;
      if (displayDataCount > 0) {
        payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
        battPercent = calculateBatteryPercentage(latest.battery_voltage);
      }
      
      // Update topbar with appropriate title for current view
      const char* title = "Live";
      switch (currentMode) {
        case LIVE_VIEW:
          title = "Live";
          updateLiveViewDynamic();
          break;
        case GRAPH_VIEW:
          title = "Graph";
          break;
        case CALIBRATION_VIEW:
          title = "Calib";
          break;
        case MORE_VIEW:
          title = "More";
          break;
        case LIVE_DETAILS_VIEW:
          title = "Details";
          break;
      }
      
      uiDrawTopbar(title, true, fermentationFileOpen, battPercent);
      topbarDirty = false;
    }
  }
  
  delay(10);
}

void initESPNow() {
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  
  // Wait a moment for WiFi to fully initialize
  delay(500);
  
  // esp_wifi_set_channel is deprecated in newer ESP-IDF versions
  // ESP-NOW will automatically use the current WiFi channel
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Register receive callback (new ESP-IDF signature)
  esp_now_register_recv_cb([](const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    onDataReceived(recv_info->src_addr, data, len);
  });
  
  Serial.println("ESP-NOW initialized");
  
  // Print base station MAC address after WiFi is fully initialized
  String macAddress = WiFi.macAddress();
  Serial.printf("Base Station MAC: %s\n", macAddress.c_str());
}

void initDisplay() {
  tft.init();
  tft.setRotation(1); // Landscape mode - MUST match touchCalibration_rotate in Touch.h (which is 1)
  
  // Verify screen dimensions after rotation
  Serial.printf("Display dimensions after rotation: %dx%d\n", tft.width(), tft.height());
  
  tft.fillScreen(currentTheme->background);
  tft.setTextColor(currentTheme->textPrimary, currentTheme->background);
  
  Serial.println("Display initialized for 480x320 landscape (rotation 1)");
}

void initTouch() {
  // TFT_eSPI touch is initialized automatically with tft.init()
  // Calibration values are set in User_Setup.h or Touch.h
  // Y-axis inversion now enabled in Touch.h (touchCalibration_invert_y = 1)
  
  // Ensure SD CS is HIGH to prevent SD card from interfering with touch
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  
  Serial.println("Touch screen ready (using TFT_eSPI built-in support)");
}

void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(payload_t)) {
    // Store float MAC address if not known yet
    if (!floatMacKnown) {
      memcpy(floatMac, mac, 6);
      floatMacKnown = true;
      Serial.printf("Float MAC address captured: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                    floatMac[0], floatMac[1], floatMac[2], floatMac[3], floatMac[4], floatMac[5]);
      
      // Save to EEPROM for permanent pairing
      saveFloatMacToEEPROM();
      
      // Register float as ESP-NOW peer for direct communication
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, floatMac, 6);
      peerInfo.channel = ESPNOW_CHANNEL;
      peerInfo.encrypt = false;
      peerInfo.ifidx = WIFI_IF_STA;
      
      esp_err_t addResult = esp_now_add_peer(&peerInfo);
      if (addResult == ESP_OK) {
        Serial.println("Float unit registered as ESP-NOW peer");
      } else if (addResult == ESP_ERR_ESPNOW_EXIST) {
        Serial.println("Float unit already registered as peer");
      } else {
        Serial.printf("Failed to register float as peer: %d\n", addResult);
      }
    }
    
    payload_t receivedData;
    memcpy(&receivedData, incomingData, len);
    
    // Verify protocol version
    if (receivedData.version != SG_PROTOCOL_VERSION) {
      Serial.printf("Warning: Protocol version mismatch (expected %d, got %d)\n", SG_PROTOCOL_VERSION, receivedData.version);
      return;
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
      Serial.printf("CRC mismatch: expected %d, got %d\n", receivedData.crc, calculated_crc);
      return;
    }
    
    uint8_t batteryPercent = calculateBatteryPercentage(receivedData.battery_voltage);
    
    Serial.printf("Received: seq=%d, angle=%.2f°, density=%.3f, temp=%.1f°C, battery=%.2fV (%d%%)\n", 
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
    
    // Add data to display buffer
    addDataPoint(receivedData);
    
    // Buffer sensor data for SD writing (time-based approach)
    bufferSensorData(receivedData);
    
    // Set region-based dirty flags for LIVE view
    if (currentMode == LIVE_VIEW) {
      // Force complete screen redraw if this is the first data point
      static bool lastHadData = false;
      bool hasData = displayDataCount > 0;
      if (!lastHadData && hasData) {
        screenDirty = true;  // Force complete redraw
      }
      lastHadData = hasData;
      
      heroDirty = true;
      tileTempDirty = true;
      tileAngleDirty = true;
      tileAbvDirty = true;
      topbarDirty = true;  // Update battery in topbar
    }
    // Also update GRAPH_VIEW if active - auto-update graph with new data
    if (currentMode == GRAPH_VIEW) {
      screenDirty = true;  // Trigger full graph redraw
    }
    // Also update LIVE_DETAILS_VIEW if active
    if (currentMode == LIVE_DETAILS_VIEW) {
      detailsDirty = true;
    }
    
    lastUpdate = millis();
    
    // Clear calibration mode flag when receiving normal data
    calibrationModeActive = false;
    
  } else if (len == sizeof(calib_command_t)) {
    // Handle calibration command from float unit (trigger)
    onCalibrationCommandFromFloat(mac, incomingData, len);
    
    // Set calibration mode flag when receiving calibration commands
    calibrationModeActive = true;
  } else if (len == sizeof(calib_response_t)) {
    // Handle calibration response from float unit
    onCalibrationResponse(mac, incomingData, len);
    
    // Set calibration mode flag when receiving calibration responses
    calibrationModeActive = true;
  } else {
    Serial.printf("Unknown data format received: %d bytes\n", len);
  }
}

void addDataPoint(payload_t data) {
  displayDataBuffer[displayDataIndex] = data;
  displayDataIndex = (displayDataIndex + 1) % MAX_DATA_POINTS;
  
  if (displayDataCount < MAX_DATA_POINTS) {
    displayDataCount++;
  }
}

void drawLiveViewStatic() {
  // Draw top bar
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar("Live", true, fermentationFileOpen, battPercent);
  
  // Draw calibration warning if active (compact)
  if (calibrationModeActive) {
    // Clear the area first to prevent artefacts
    tft.fillRect(MARGIN - 2, TOPBAR_H + 2, UI_W - MARGIN * 2 + 4, 20, uiColorBackground);
    tft.setTextColor(uiColorError);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN, TOPBAR_H + 6);  // Adjust for FreeFont baseline
    tft.print("CALIB MODE");
  }
  
  int contentY = calibrationModeActive ? TOPBAR_H + 14 : TOPBAR_H;
  int contentH = UI_H - contentY - NAV_H;
  
  // No data state
  if (displayDataCount == 0) {
    int cardX = UI_W / 2 - 100;
    int cardY = contentY + contentH / 2 - 40;
    uiCard(cardX, cardY, 200, 80, CARD_RADIUS);
    
    // Centered error message
    uiTextCenter(cardX, cardY + 20, 200, 20, "No Data received", FONT_SIZE_SM, uiColorTextMuted);
    uiTextCenter(cardX, cardY + 45, 200, 20, "Waiting...", FONT_SIZE_SM, uiColorTextMuted);
    
    uiDrawBottomNav(TAB_LIVE);
    return;
  }
  
  // Hero card (full width)
  payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
  int heroH = contentH / 2;
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, heroH, CARD_RADIUS);
  
  // Prepare sparkline data (last ~20 SG readings)
  float sparklineData[20];
  int sparklineCount = 0;
  int pointsToShow = min(displayDataCount, 20);
  for (int i = 0; i < pointsToShow; i++) {
    int idx = (displayDataIndex - pointsToShow + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    sparklineData[i] = displayDataBuffer[idx].density;
    sparklineCount++;
  }
  
  uiHeroSG(MARGIN, contentY, UI_W - MARGIN * 2, heroH, latest.density, "", sparklineData, sparklineCount);
  
  // 3 tiles below hero (Temp, Angle, ABV) - Batt moved to LIVE_DETAILS_VIEW
  int tileY = contentY + heroH + GAP;
  int tileW = (UI_W - MARGIN * 2 - GAP * 2) / 3;  // 3 tiles with gaps
  int tileH = contentH - heroH - GAP;
  
  uiTile(MARGIN, tileY, tileW, tileH, 0, "Temp", "--", "°C", false);
  uiTile(MARGIN + tileW + GAP, tileY, tileW, tileH, 0, "Angle", "--", "°", false);
  uiTile(MARGIN + tileW * 2 + GAP * 2, tileY, tileW, tileH, 0, "ABV", "Set OG", "%", true);
  
  // Bottom navigation
  uiDrawBottomNav(TAB_LIVE);
  
  // Reset dirty flags
  heroDirty = true;
  tileTempDirty = true;
  tileAngleDirty = true;
  tileAbvDirty = true;
}

void updateLiveViewDynamic() {
  if (displayDataCount == 0) {
    return;
  }
  
  // Get latest data
  payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
  uint8_t batteryPercent = calculateBatteryPercentage(latest.battery_voltage);
  
  int contentY = calibrationModeActive ? TOPBAR_H + 14 : TOPBAR_H;
  int contentH = UI_H - contentY - NAV_H;
  int heroH = contentH / 2;
  int tileY = contentY + heroH + GAP;
  int tileW = (UI_W - MARGIN * 2 - GAP * 2) / 3;  // 3 tiles
  int tileH = contentH - heroH - GAP;
  
  char buffer[16];
  
  // Update hero SG value with trend and sparkline
  if (heroDirty) {
    // Calculate trend (compare latest with ~5 min ago, assuming ~30s intervals = 10 points back)
    char trendBuf[16] = "";
    if (displayDataCount >= 10) {
      int oldIdx = (displayDataIndex - 10 + MAX_DATA_POINTS) % MAX_DATA_POINTS;
      float trend = latest.density - displayDataBuffer[oldIdx].density;
      if (abs(trend) >= 0.001) {
        snprintf(trendBuf, sizeof(trendBuf), "%+.3f", trend);
      }
    }
    
    // Prepare sparkline data (last ~20 SG readings)
    float sparklineData[20];
    int sparklineCount = 0;
    int pointsToShow = min(displayDataCount, 20);
    for (int i = 0; i < pointsToShow; i++) {
      int idx = (displayDataIndex - pointsToShow + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
      sparklineData[i] = displayDataBuffer[idx].density;
      sparklineCount++;
    }
    
    uiHeroSG(MARGIN, contentY, UI_W - MARGIN * 2, heroH, latest.density, 
             trendBuf[0] ? trendBuf : "", sparklineData, sparklineCount);
    heroDirty = false;
  }
  
  // Update temp tile
  if (tileTempDirty) {
    snprintf(buffer, sizeof(buffer), "%.1f", latest.temperature);
    uiTile(MARGIN, tileY, tileW, tileH, 0, "Temp", buffer, "°C", false);
    tileTempDirty = false;
  }
  
  // Update angle tile
  if (tileAngleDirty) {
    snprintf(buffer, sizeof(buffer), "%.1f", latest.angle);
    uiTile(MARGIN + tileW + GAP, tileY, tileW, tileH, 0, "Angle", buffer, "°", false);
    tileAngleDirty = false;
  }
  
  // Update ABV tile
  if (tileAbvDirty) {
    if (ogCaptured) {
      snprintf(buffer, sizeof(buffer), "%.1f", currentABV);
      uiTile(MARGIN + tileW * 2 + GAP * 2, tileY, tileW, tileH, 0, "ABV", buffer, "%", false);
    } else {
      uiTile(MARGIN + tileW * 2 + GAP * 2, tileY, tileW, tileH, 0, "ABV", "Set OG", "%", true);
    }
    tileAbvDirty = false;
  }
}

void drawLiveView() {
  // Check if we're transitioning from no-data to data state
  static bool lastHadData = false;
  bool hasData = displayDataCount > 0;
  
  // Force complete screen clear on data state transitions to prevent artefacts
  if (!staticElementsDrawn || currentMode != LIVE_VIEW || 
      (lastHadData && !hasData) || (!lastHadData && hasData)) {
    tft.fillScreen(uiColorBackground);
    // Also clear any calibration warning area that might remain
    if (!hasData && calibrationModeActive) {
      tft.fillRect(MARGIN - 2, TOPBAR_H + 2, UI_W - MARGIN * 2 + 4, 20, uiColorBackground);
    }
    staticElementsDrawn = false;  // Force full redraw
  }
  
  // Always redraw static elements when needed
  if (!staticElementsDrawn) {
    drawLiveViewStatic();
    staticElementsDrawn = true;
  }
  
  updateLiveViewDynamic();
  
  lastHadData = hasData;
}

void drawGraphView() {
  tft.fillScreen(uiColorBackground);
  
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar("Graph", true, fermentationFileOpen, battPercent);
  
  int contentY = TOPBAR_H;
  int contentH = UI_H - contentY - NAV_H;
  
  if (displayDataCount < 3) {
    uiCard(MARGIN, contentY + contentH / 2 - 40, UI_W - MARGIN * 2, 80, CARD_RADIUS);
    uiTextCenter(MARGIN, contentY + contentH / 2 - 10, UI_W - MARGIN * 2, 20, "No graph data", FONT_SIZE_MD, uiColorTextMuted);
    uiTextCenter(MARGIN, contentY + contentH / 2 + 10, UI_W - MARGIN * 2, 20, "Collect a few points first", FONT_SIZE_SM, uiColorTextMuted);
    uiDrawBottomNav(TAB_GRAPH);
    return;
  }
  
  // Draw graph card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, contentH, CARD_RADIUS);
  drawGraph();
  
  uiDrawBottomNav(TAB_GRAPH);
}

void drawGrid(int graphX, int graphY, int graphW, int graphH, float minDensity, float maxDensity) {
  // Draw subtle grid lines
  // Horizontal grid lines (4 lines)
  for (int i = 1; i < 4; i++) {
    int y = graphY + (i * graphH / 4);
    tft.drawFastHLine(graphX, y, graphW, uiColorBorder);
  }
  
  // Vertical grid lines (4 lines)
  for (int i = 1; i < 4; i++) {
    int x = graphX + (i * graphW / 4);
    tft.drawFastVLine(x, graphY, graphH, uiColorBorder);
  }
  
  // Draw Y-axis label
  tft.setTextColor(uiColorTextMuted);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(graphX - 25, graphY + 10);
  tft.print("SG");
  
  // Draw Y-axis min/max values
  char buf[16];
  snprintf(buf, sizeof(buf), "%.3f", maxDensity);
  tft.setCursor(graphX - 35, graphY + 5);
  tft.print(buf);
  
  snprintf(buf, sizeof(buf), "%.3f", minDensity);
  tft.setCursor(graphX - 35, graphY + graphH);
  tft.print(buf);
}

void drawGraph() {
  if (displayDataCount < 2) return;
  
  // Graph area inside the card with margins for axis labels
  int axisMarginLeft = 45;
  int axisMarginBottom = 25;
  int graphX = MARGIN + 10 + axisMarginLeft;
  int graphY = TOPBAR_H + 10;
  int graphW = UI_W - MARGIN * 2 - 20 - axisMarginLeft;
  int graphH = UI_H - TOPBAR_H - NAV_H - 20 - axisMarginBottom;
  
  // Find min/max density for scaling
  float minDensity = 1.0, maxDensity = 1.1;
  
  for (int i = 0; i < displayDataCount; i++) {
    payload_t data = displayDataBuffer[(displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    if (data.density < minDensity) minDensity = data.density;
    if (data.density > maxDensity) maxDensity = data.density;
  }
  
  // Add padding
  minDensity -= 0.002;
  maxDensity += 0.002;
  
  // Draw grid with axis labels
  drawGrid(graphX, graphY, graphW, graphH, minDensity, maxDensity);
  
  // Calculate time range for X-axis labels
  if (displayDataCount >= 2) {
    payload_t firstData = displayDataBuffer[(displayDataIndex - displayDataCount + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    payload_t lastData = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    uint32_t timeSpan = lastData.uptime_s - firstData.uptime_s;
    
    // Draw X-axis label
    tft.setTextColor(uiColorTextMuted);
    tft.setFreeFont(FONT_SIZE_XS);
    tft.setCursor(graphX + graphW / 2 - 15, graphY + graphH + axisMarginBottom - 5);
    tft.print("Time");
    
    // Draw X-axis time labels (start and end times)
    char timeBuf[16];
    if (timeSpan < 3600) {
      // Show minutes
      snprintf(timeBuf, sizeof(timeBuf), "%dmin", (int)(firstData.uptime_s / 60));
      tft.setCursor(graphX - 5, graphY + graphH + 15);
      tft.print(timeBuf);
      snprintf(timeBuf, sizeof(timeBuf), "%dmin", (int)(lastData.uptime_s / 60));
      tft.setCursor(graphX + graphW - 30, graphY + graphH + 15);
      tft.print(timeBuf);
    } else {
      // Show hours
      snprintf(timeBuf, sizeof(timeBuf), "%.1fh", firstData.uptime_s / 3600.0);
      tft.setCursor(graphX - 5, graphY + graphH + 15);
      tft.print(timeBuf);
      snprintf(timeBuf, sizeof(timeBuf), "%.1fh", lastData.uptime_s / 3600.0);
      tft.setCursor(graphX + graphW - 30, graphY + graphH + 15);
      tft.print(timeBuf);
    }
  }

  // Prevent division by zero
  float densityRange = maxDensity - minDensity;
  if (densityRange == 0) densityRange = 0.001;

  // Draw gold polyline
  int prevX = -1, prevY = -1;

  for (int i = 0; i < displayDataCount; i++) {
    payload_t data = displayDataBuffer[(displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS];

    int x = graphX + (i * graphW / (displayDataCount - 1));
    int y = graphY + graphH - ((data.density - minDensity) / densityRange * graphH);

    if (prevX >= 0 && prevY >= 0) {
      tft.drawLine(prevX, prevY, x, y, uiColorGold);
    }
    
    prevX = x;
    prevY = y;
  }
  
  // Draw last point dot
  if (displayDataCount > 0 && prevX >= 0 && prevY >= 0) {
    tft.fillCircle(prevX, prevY, 4, uiColorGold);
  }
}

void drawCalibrationView() {
  tft.fillScreen(uiColorBackground);
  
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar("Calib", true, fermentationFileOpen, battPercent);
  
  int contentY = TOPBAR_H;
  int contentH = UI_H - TOPBAR_H - NAV_H;
  
  // Calibration warning (compact)
  tft.setTextColor(uiColorError);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN, contentY + 6);  // Adjust for FreeFont baseline
  tft.print("HIGH BATTERY");
  contentY += 14;
  
  // Main content card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, contentH - 50, CARD_RADIUS);
  
  if (calibMode == CALIB_IDLE) {
    uiTextCenter(MARGIN, contentY + 20, UI_W - MARGIN * 2, 24, "Calibration", FONT_SIZE_LG, uiColorTextPrimary);
    drawButton(BUTTON_CALIB_START_X, BUTTON_CALIB_START_Y, BUTTON_CALIB_START_W, BUTTON_CALIB_START_H, "START", false);
    
  } else if (calibMode == CALIB_INSTRUCTIONS) {
    // Calculate proper text positions using font metrics
    tft.setTextColor(uiColorAccent);
    tft.setFreeFont(FONT_SIZE_MD);
    int stepsY = contentY + 10 + tft.fontHeight(); // Proper baseline positioning
    tft.setCursor(MARGIN + 10, stepsY);
    tft.print("Steps:");
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4; // Line spacing
    int bullet1Y = stepsY + lineHeight;
    int bullet2Y = bullet1Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.print("• Bowl (3L+) + 2L water");
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("• Place float, wait");
    drawButton(BUTTON_CALIB_NEXT_X, BUTTON_CALIB_NEXT_Y, BUTTON_CALIB_NEXT_W, BUTTON_CALIB_NEXT_H, "NEXT", false);
    
  } else if (calibMode == CALIB_POINT1) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Step 1/4 - SG 1.000");
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, buf, FONT_SIZE_MD, uiColorAccent);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 40 + tft.fontHeight(); // Proper baseline
    int bullet2Y = bullet1Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.print("• Float in water");
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("• Wait, then RECORD");
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "RECORD", calibAngles[0] > 0);
    
  } else if (calibMode == CALIB_POINT2) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Step 2/4 - SG 1.040");
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, buf, FONT_SIZE_MD, uiColorAccent);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 40 + tft.fontHeight(); // Proper baseline
    int bullet2Y = bullet1Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.printf("• Add %dg sugar", sugarAmounts[1]);
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("• Dissolve, RECORD");
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "RECORD", calibAngles[1] > 0);
    
  } else if (calibMode == CALIB_POINT3) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Step 3/4 - SG 1.080");
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, buf, FONT_SIZE_MD, uiColorAccent);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 40 + tft.fontHeight(); // Proper baseline
    int bullet2Y = bullet1Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.printf("• Add %dg sugar", sugarAmounts[2]);
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("• Dissolve, RECORD");
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "RECORD", calibAngles[2] > 0);
    
  } else if (calibMode == CALIB_POINT4) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Step 4/4 - SG 1.120");
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, buf, FONT_SIZE_MD, uiColorAccent);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 40 + tft.fontHeight(); // Proper baseline
    int bullet2Y = bullet1Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.printf("• Add %dg sugar", sugarAmounts[3]);
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("• Dissolve, RECORD");
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "RECORD", calibAngles[3] > 0);
    
  } else if (calibMode == CALIB_COMPLETE) {
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, "4 Points Recorded", FONT_SIZE_MD, uiColorTextPrimary);
    
    char buf[64];
    snprintf(buf, sizeof(buf), "P1:%.1f P2:%.1f", calibAngles[0], calibAngles[1]);
    tft.setTextColor(uiColorAccent);
    tft.setFreeFont(FONT_SIZE_SM);
    int line1Y = contentY + 40 + tft.fontHeight(); // Proper baseline
    int line2Y = line1Y + tft.fontHeight() + 4; // Line spacing
    
    tft.setCursor(MARGIN + 10, line1Y);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "P3:%.1f P4:%.1f", calibAngles[2], calibAngles[3]);
    tft.setCursor(MARGIN + 10, line2Y);
    tft.print(buf);
    drawButton(BUTTON_CALIB_APPLY_X, BUTTON_CALIB_APPLY_Y, BUTTON_CALIB_APPLY_W, BUTTON_CALIB_APPLY_H, "APPLY", false);
  } else if (calibMode == CALIB_APPLYING) {
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, "Applying Calibration", FONT_SIZE_MD, uiColorTextPrimary);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN + 10, contentY + 50 + tft.fontHeight());
    tft.print("Calculating polynomial...");
    
  } else if (calibMode == CALIB_FAILED) {
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, "CALIBRATION FAILED", FONT_SIZE_MD, uiColorError);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN + 10, contentY + 50 + tft.fontHeight());
    tft.print("No response from float");
    
    drawButton(BUTTON_CALIB_EXIT_X, BUTTON_CALIB_EXIT_Y, BUTTON_CALIB_EXIT_W, BUTTON_CALIB_EXIT_H, "EXIT", false);
  }
  
  // Exit button (not shown in applying state)
  if (calibMode != CALIB_APPLYING) {
    drawButton(BUTTON_CALIB_EXIT_X, BUTTON_CALIB_EXIT_Y, BUTTON_CALIB_EXIT_W, BUTTON_CALIB_EXIT_H, "EXIT", false);
  }
  
  uiDrawBottomNav(TAB_CALIB);
  
  // Draw wait notification overlay if active
  if (waitActive) {
    // Semi-transparent overlay
    tft.fillScreen(0x0000); // Black overlay
    
    // Wait message box
    int boxW = 200;
    int boxH = 80;
    int boxX = (SCREEN_W - boxW) / 2;
    int boxY = (SCREEN_H - boxH) / 2;
    
    uiCard(boxX, boxY, boxW, boxH, CARD_RADIUS);
    
    tft.setTextColor(uiColorTextPrimary);
    tft.setFreeFont(FONT_SIZE_MD);
    uiTextCenter(boxX, boxY + 25, boxW, 20, "WAIT", FONT_SIZE_MD, uiColorTextPrimary);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    uiTextCenter(boxX, boxY + 50, boxW, 16, "Measuring...", FONT_SIZE_SM, uiColorTextSecondary);
  }
}

// UI Design System Helper Functions Implementation

void drawHeader(const char* title) {
  // Draw header background
  tft.fillRect(0, 0, SCREEN_W, 40, currentTheme->primary);
  
  // Draw SGNode branding
  tft.setTextColor(currentTheme->primaryText);
  tft.setFreeFont(FONT_SIZE_MD);
  tft.setCursor(10, 16);  // Adjust for FreeFont baseline
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
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(10, SCREEN_H - 16);  // Adjust for FreeFont baseline
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
  tft.setFreeFont(FONT_SIZE_SM);
  int textLen = strlen(label);
  int16_t w1 = textLen * 6; // Approximate width (6 pixels per character)
  int16_t h1 = 8; // Approximate height for text size 1
  tft.setCursor(x + (w - w1) / 2, y + (h - h1) / 2 + 4);  // Adjust for FreeFont baseline
  tft.print(label); // Changed from println to print
}

void toggleTheme() {
  darkMode = !darkMode;
  currentTheme = darkMode ? &darkTheme : &lightTheme;
  
  // Redraw entire screen with new theme
  tft.fillScreen(currentTheme->background);
  screenDirty = true;
}

void drawBootScreen() {
  tft.fillScreen(currentTheme->background);
  
  // Center logo on screen
  int logoX = (SCREEN_W - logo2_width) / 2;
  int logoY = (SCREEN_H - logo2_height) / 2 - 20; // Move up to make room for text
  
  // Draw logo from optimized pixel data (only non-white pixels stored)
  // Count pixels in the array (calculated from array size)
  int numPixels = sizeof(logo2_pixels) / sizeof(Pixel);
  for (int i = 0; i < numPixels; i++) {
    int x = logoX + logo2_pixels[i].x;
    int y = logoY + logo2_pixels[i].y;
    uint16_t color = logo2_pixels[i].color;
    tft.drawPixel(x, y, color);
  }
  
  // Draw version text below logo
  tft.setTextColor(currentTheme->textSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(logoX + 60, logoY + logo2_height + 14);  // Adjust for FreeFont baseline
  tft.println("SGNode Base Station");
  
  delay(2000); // Show boot screen for 2 seconds
}

void checkTouch() {
  uint16_t x, y;
  
  // Use TFT_eSPI built-in touch (calibrated in Touch.h)
  bool touched = tft.getTouch(&x, &y);
  
  if (touched) {
    // Update last touch time for SD operation timing
    lastTouchTime = millis();
    
    touchX = x;
    touchY = y;
    
    // Handle Create New Fermentation dialog touches (highest priority)
    if (showingCreateNewDialog) {
      // Yes button
      if (touchX >= BUTTON_DIALOG_YES_X && touchX <= BUTTON_DIALOG_YES_X + BUTTON_DIALOG_YES_W &&
          touchY >= BUTTON_DIALOG_YES_Y && touchY <= BUTTON_DIALOG_YES_Y + BUTTON_DIALOG_YES_H) {
        createNewFermentationFile();
        showingCreateNewDialog = false;
        screenDirty = true;
        Serial.println("Created new fermentation from dialog");
      }
      
      // Back button
      if (touchX >= BUTTON_DIALOG_BACK_X && touchX <= BUTTON_DIALOG_BACK_X + BUTTON_DIALOG_BACK_W &&
          touchY >= BUTTON_DIALOG_BACK_Y && touchY <= BUTTON_DIALOG_BACK_Y + BUTTON_DIALOG_BACK_H) {
        showingCreateNewDialog = false;
        screenDirty = true;
        Serial.println("Cancelled new fermentation from dialog");
      }
      
      delay(50);
      return;
    }
    
    // Check bottom navigation using helper
    int tab = uiNavHitTest(touchX, touchY);
    if (tab >= 0) {
      if (tab != currentMode) {
        switch (tab) {
          case TAB_LIVE:
            currentMode = LIVE_VIEW;
            staticElementsDrawn = false;  // Force full redraw
            screenDirty = true;
            Serial.println("Switched to Live View");
            break;
          case TAB_GRAPH:
            currentMode = GRAPH_VIEW;
            screenDirty = true;
            Serial.println("Switched to Graph View");
            break;
          case TAB_CALIB:
            currentMode = CALIBRATION_VIEW;
            screenDirty = true;
            Serial.println("Switched to Calibration View");
            break;
          case TAB_MORE:
            currentMode = MORE_VIEW;
            screenDirty = true;
            Serial.println("Switched to More View");
            break;
        }
      }
      delay(50);
      return;
    }
    
    // Handle More view card taps
    if (currentMode == MORE_VIEW) {
      int contentY = TOPBAR_H + MARGIN;
      int cardH = (UI_H - TOPBAR_H - NAV_H - MARGIN * 2 - GAP * 3) / 4;
      int cardW = UI_W - MARGIN * 2;
      
      // System card (goes to LIVE_DETAILS)
      if (touchX >= MARGIN && touchX <= MARGIN + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        currentMode = LIVE_DETAILS_VIEW;
        screenDirty = true;
        Serial.println("Switched to Live Details");
      }
      
      // Set OG card
      contentY += cardH + GAP;
      if (!ogCaptured && touchX >= MARGIN && touchX <= MARGIN + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        // Set OG from latest reading
        if (displayDataCount > 0) {
          payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
          originalGravity = latest.density;
          ogCaptured = true;
          currentABV = calculateABV(originalGravity, latest.density);
          screenDirty = true;
          Serial.printf("Set OG to: %.3f\n", originalGravity);
        }
      }
      
      // Create New Fermentation card
      contentY += cardH + GAP;
      if (touchX >= MARGIN && touchX <= MARGIN + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        showingCreateNewDialog = true;
        screenDirty = true;
        Serial.println("Opened Create New Fermentation dialog");
      }
      
      // Reboot card
      contentY += cardH + GAP;
      if (touchX >= MARGIN && touchX <= MARGIN + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        Serial.println("Rebooting...");
        ESP.restart();
      }
    }
    
    // Handle Live Details SET OG button
    if (currentMode == LIVE_DETAILS_VIEW && !ogCaptured && displayDataCount > 0) {
      int contentY = TOPBAR_H + MARGIN;
      int contentW = UI_W - MARGIN * 2;
      int headerH = 50;
      int cardH = 80;
      int logH = 70;
      int btnY = contentY + headerH + GAP + cardH + GAP + logH + GAP;
      
      if (touchX >= MARGIN && touchX <= MARGIN + contentW &&
          touchY >= btnY && touchY <= btnY + 40) {
        payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
        originalGravity = latest.density;
        ogCaptured = true;
        currentABV = calculateABV(originalGravity, latest.density);
        screenDirty = true;
        Serial.printf("Set OG to: %.3f\n", originalGravity);
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
        Serial.println("Started calibration wizard");
      }
      
      // Next button (instructions to step 1)
      if (calibMode == CALIB_INSTRUCTIONS &&
          touchX >= BUTTON_CALIB_NEXT_X && touchX <= BUTTON_CALIB_NEXT_X + BUTTON_CALIB_NEXT_W &&
          touchY >= BUTTON_CALIB_NEXT_Y && touchY <= BUTTON_CALIB_NEXT_Y + BUTTON_CALIB_NEXT_H) {
        calibMode = CALIB_POINT1;
        screenDirty = true;
        Serial.println("Moved to Step 1");
      }
      
      // Record button for each step - new workflow with wait
      if (calibMode == CALIB_POINT1 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(0, 1.000);
        startWait(2000); // 2 second wait for RECORD
        Serial.println("RECORD Point 1 - started 2s wait");
      }
      
      if (calibMode == CALIB_POINT2 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(1, 1.040);
        startWait(2000); // 2 second wait for RECORD
        Serial.println("RECORD Point 2 - started 2s wait");
      }
      
      if (calibMode == CALIB_POINT3 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(2, 1.080);
        startWait(2000); // 2 second wait for RECORD
        Serial.println("RECORD Point 3 - started 2s wait");
      }
      
      if (calibMode == CALIB_POINT4 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(3, 1.120);
        startWait(2000); // 2 second wait for RECORD
        Serial.println("RECORD Point 4 - started 2s wait");
      }
      
      // Apply button - new workflow with 3s wait and failure handling
      if (calibMode == CALIB_COMPLETE &&
          touchX >= BUTTON_CALIB_APPLY_X && touchX <= BUTTON_CALIB_APPLY_X + BUTTON_CALIB_APPLY_W &&
          touchY >= BUTTON_CALIB_APPLY_Y && touchY <= BUTTON_CALIB_APPLY_Y + BUTTON_CALIB_APPLY_H) {
        calibMode = CALIB_APPLYING;
        applyCalibration();
        startWait(3000); // 3 second wait for APPLY
        screenDirty = true;
        Serial.println("APPLY - started 3s wait for confirmation");
      }
      
      // Exit button - all exits go to Live view
      if (touchX >= BUTTON_CALIB_EXIT_X && touchX <= BUTTON_CALIB_EXIT_X + BUTTON_CALIB_EXIT_W &&
          touchY >= BUTTON_CALIB_EXIT_Y && touchY <= BUTTON_CALIB_EXIT_Y + BUTTON_CALIB_EXIT_H) {
        currentMode = LIVE_VIEW;
        calibMode = CALIB_IDLE;
        calibrationModeActive = false;
        stopWait(); // Stop any active wait
        // Reset calibration data
        for (int i = 0; i < 4; i++) calibAngles[i] = 0.0;
        screenDirty = true;
        Serial.println("EXIT calibration - returned to Live view");
        Serial.println("Exited calibration mode");
      }
    }
    
    delay(50); // Debounce
  }
}

void sendCalibrationCommand(uint8_t command, float target_sg) {
  // Send calibration command to float unit with retry logic
  calib_command_t calibCmd;
  calibCmd.command = command;
  calibCmd.target_sg = target_sg;
  calibCmd.request_id = command; // Use command as request_id for simplicity
  
  // Send with retry logic
  esp_err_t result;
  int maxRetries = 3;
  
  for (int retry = 0; retry < maxRetries; retry++) {
    if (floatMacKnown) {
      result = esp_now_send(floatMac, (uint8_t*)&calibCmd, sizeof(calibCmd));
      if (retry == 0) {
        Serial.printf("Sent calibration command %d directly to float (MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n", 
                      command, floatMac[0], floatMac[1], floatMac[2], floatMac[3], floatMac[4], floatMac[5]);
      }
    } else {
      result = esp_now_send(NULL, (uint8_t*)&calibCmd, sizeof(calibCmd));
      if (retry == 0) {
        Serial.printf("Sent calibration command %d via broadcast (float MAC unknown)\n", command);
      }
    }
    
    if (result == ESP_OK) {
      break; // Success, no need to retry
    } else {
      if (retry < maxRetries - 1) {
        Serial.printf("Calibration command send failed (attempt %d/%d): %d\n", retry + 1, maxRetries, result);
        delay(100); // Wait before retry
      } else {
        Serial.printf("Failed to send calibration command after %d attempts: %d\n", maxRetries, result);
      }
    }
  }
}

void applyCalibration() {
  // Apply calibration - send command to calculate polynomial coefficients
  calib_command_t calibCmd;
  calibCmd.command = 4; // APPLY_CALIBRATION
  calibCmd.target_sg = 0.0;
  calibCmd.request_id = currentRequestId++;
  
  // Send to float unit directly if MAC is known, otherwise broadcast
  esp_err_t result;
  if (floatMacKnown) {
    result = esp_now_send(floatMac, (uint8_t*)&calibCmd, sizeof(calibCmd));
    Serial.printf("Sent apply calibration command directly to float (MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n", 
                  floatMac[0], floatMac[1], floatMac[2], floatMac[3], floatMac[4], floatMac[5]);
  } else {
    result = esp_now_send(NULL, (uint8_t*)&calibCmd, sizeof(calibCmd));
    Serial.println("Sent apply calibration command via broadcast (float MAC unknown)");
  }
  
  if (result != ESP_OK) {
    Serial.printf("Error sending apply calibration command: %d\n", result);
  }
}

void onCalibrationResponse(const uint8_t *mac, const uint8_t *incomingData, int len) {
  // Flood protection: Check rate limiting
  unsigned long currentTime = millis();
  if (currentTime - lastCalibrationResponse < CALIBRATION_RESPONSE_RATE_LIMIT) {
    Serial.printf("Calibration response rate limited (dropped)\n");
    return;
  }
  
  // Update rate limiting counters
  lastCalibrationResponse = currentTime;
  calibrationResponseCount++;
  
  // Reset counter every minute to prevent permanent blocking
  static unsigned long lastCounterReset = 0;
  if (currentTime - lastCounterReset > 60000) { // 1 minute
    calibrationResponseCount = 0;
    lastCounterReset = currentTime;
  }
  
  // Block if too many responses in one minute
  if (calibrationResponseCount > MAX_CALIBRATION_RESPONSES_PER_MINUTE) {
    Serial.printf("Calibration response flood detected - blocking responses\n");
    return;
  }
  
  // Handle calibration response from float unit
  if (len != sizeof(calib_response_t)) {
    Serial.printf("Invalid calibration response size: %d bytes (expected %d)\n", len, sizeof(calib_response_t));
    return;
  }
  
  calib_response_t calibResp;
  memcpy(&calibResp, incomingData, len);
  
  Serial.printf("Calibration response: angle=%.2f°, sg=%.3f, request_id=%d\n", 
                calibResp.angle, calibResp.sg, calibResp.request_id);
  
  if (calibResp.response_type == 0) {
    // Simplified response handling for new workflow
    // Handle apply response (request_id=4) - polynomial calculation complete
    if (calibResp.request_id == 4 && calibMode == CALIB_APPLYING) {
      Serial.println("Calibration apply completed - exiting calibration mode");
      stopWait(); // Stop the 3s wait
      calibMode = CALIB_IDLE;
      currentMode = LIVE_VIEW; // Exit calibration view
      calibrationModeActive = false;
      screenDirty = true;
      staticElementsDrawn = false; // Force full redraw
      Serial.println("Switched to Live View after calibration completion");
    } else {
      // Calibration data - store angle based on which point was calibrated
      // Determine which calibration point this response is for based on target_sg
      if (calibResp.sg >= 0.995 && calibResp.sg <= 1.005) {
        // Point 1 - Water (SG=1.000)
        calibAngles[0] = calibResp.angle;
        Serial.printf("Stored Point 1 angle: %.2f°\n", calibAngles[0]);
        // Note: State advancement is now handled by wait timeout, not response
      } else if (calibResp.sg >= 1.035 && calibResp.sg <= 1.045) {
        // Point 2 - Light Sugar (SG=1.040)
        calibAngles[1] = calibResp.angle;
        Serial.printf("Stored Point 2 angle: %.2f°\n", calibAngles[1]);
        // Note: State advancement is now handled by wait timeout, not response
      } else if (calibResp.sg >= 1.075 && calibResp.sg <= 1.085) {
        // Point 3 - Medium Sugar (SG=1.080)
        calibAngles[2] = calibResp.angle;
        Serial.printf("Stored Point 3 angle: %.2f°\n", calibAngles[2]);
        // Note: State advancement is now handled by wait timeout, not response
      } else if (calibResp.sg >= 1.115 && calibResp.sg <= 1.125) {
        // Point 4 - Heavy Sugar (SG=1.120)
        calibAngles[3] = calibResp.angle;
        Serial.printf("Stored Point 4 angle: %.2f°\n", calibAngles[3]);
        // Note: State advancement is now handled by wait timeout, not response
      }
    }
  }
}

// Handle calibration commands from float unit (triggers)
void onCalibrationCommandFromFloat(const uint8_t *mac, const uint8_t *incomingData, int len) {
  Serial.println("=== RECEIVED CALIBRATION COMMAND FROM FLOAT ===");
  if (len != sizeof(calib_command_t)) {
    Serial.printf("Invalid calibration command size from float: %d bytes (expected %d)\n", len, sizeof(calib_command_t));
    return;
  }
  
  calib_command_t cmd;
  memcpy(&cmd, incomingData, len);
  
  Serial.printf("Received calibration command from float: %d, target_sg=%.3f, request_id=%d\n", 
                cmd.command, cmd.target_sg, cmd.request_id);
  
  if (cmd.command == 5 && cmd.request_id == 255) {
    // CALIBRATION_TRIGGER - float entered calibration mode
    Serial.println("Float entered calibration mode - starting calibration sequence");
    
    // Capture float MAC if not known yet (trigger might come before data)
    if (!floatMacKnown) {
      memcpy(floatMac, mac, 6);
      floatMacKnown = true;
      Serial.printf("Float MAC captured from trigger: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                    floatMac[0], floatMac[1], floatMac[2], floatMac[3], floatMac[4], floatMac[5]);
      
      // Save to EEPROM for permanent pairing
      saveFloatMacToEEPROM();
      
      // Register float as ESP-NOW peer for direct communication
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, floatMac, 6);
      peerInfo.channel = ESPNOW_CHANNEL;
      peerInfo.encrypt = false;
      peerInfo.ifidx = WIFI_IF_STA;
      
      esp_err_t addResult = esp_now_add_peer(&peerInfo);
      if (addResult == ESP_OK) {
        Serial.println("Float unit registered as ESP-NOW peer (from trigger)");
      } else if (addResult == ESP_ERR_ESPNOW_EXIST) {
        Serial.println("Float unit already registered as peer");
      } else {
        Serial.printf("Failed to register float as peer: %d\n", addResult);
      }
    }
    
    calibrationModeActive = true;
    
    // Switch to calibration view and show instructions (wait for user to start)
    currentMode = CALIBRATION_VIEW;
    calibMode = CALIB_INSTRUCTIONS;
    screenDirty = true;
    Serial.println("=== SWITCHING TO CALIBRATION VIEW ===");
    Serial.println("Float entered calibration mode - ready for user to start calibration");
  }
}

void initSDCard() {
  #if SD_ENABLED
    Serial.println("Initializing SD card (shares SPI with TFT)...");
    
    // SD shares SPI with TFT - TFT_eSPI already initialized SPI
    // Just set SD_CS high initially to prevent conflicts
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    
    if (!SD.begin(SD_CS)) {
      Serial.println("SD card initialization failed!");
      tft.setTextColor(currentTheme->error);
      tft.setFreeFont(FONT_SIZE_SM);
      tft.setCursor(10, 104);  // Adjust for FreeFont baseline
      tft.println("SD Card Failed!");
      return;
    }
    
    Serial.println("SD card initialized successfully");
    
    // Check if fermentation directory exists
    if (!SD.exists("/fermentation")) {
      SD.mkdir("/fermentation");
      Serial.println("Created fermentation directory");
    }
  #else
    Serial.println("SD card support compiled out (SD_ENABLED=false)");
  #endif
}

bool loadHistoricalDataFromCSV(const char* filename) {
  #if !SD_ENABLED
    return false;
  #endif
  
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.printf("Failed to open CSV file for reading: %s\n", filename);
    return false;
  }
  
  Serial.printf("Loading historical data from: %s\n", filename);
  
  // Reset display data buffer
  displayDataIndex = 0;
  displayDataCount = 0;
  
  char line[128];
  int lineNum = 0;
  int loadedCount = 0;
  
  // Read and parse CSV file
  while (file.available() && loadedCount < MAX_DATA_POINTS) {
    int bytesRead = file.readBytesUntil('\n', line, sizeof(line) - 1);
    line[bytesRead] = '\0';
    lineNum++;
    
    // Skip header line and comment lines
    if (lineNum == 1 || line[0] == '#') {
      continue;
    }
    
    // Parse CSV line: uptime_s,angle,density,temperature,battery_voltage,battery_percent
    payload_t data = {0};
    uint8_t battery_percent = 0; // Temporary variable for CSV parsing
    int parsed = sscanf(line, "%lu,%f,%f,%f,%f,%hhu",
                       &data.uptime_s,
                       &data.angle,
                       &data.density,
                       &data.temperature,
                       &data.battery_voltage,
                       &battery_percent);
    
    if (parsed == 6) {
      // Add to display buffer
      displayDataBuffer[displayDataIndex] = data;
      displayDataIndex = (displayDataIndex + 1) % MAX_DATA_POINTS;
      if (displayDataCount < MAX_DATA_POINTS) {
        displayDataCount++;
      }
      loadedCount++;
      
      // Calculate battery percentage for display
      uint8_t battPercent = calculateBatteryPercentage(data.battery_voltage);
      
      // Check for OG capture
      if (!ogCaptured && data.density > 1.000) {
        originalGravity = data.density;
        ogCaptured = true;
        Serial.printf("Loaded OG from CSV: %.4f\n", originalGravity);
      }
      
      // Debug output for first few points
      if (loadedCount <= 3) {
        Serial.printf("Loaded point %d: SG=%.4f, Temp=%.1f°C, Batt=%.2fV (%d%%)\n",
                     loadedCount, data.density, data.temperature, data.battery_voltage, battPercent);
      }
    } else if (strlen(line) > 0) {
      Serial.printf("Warning: Failed to parse line %d: %s\n", lineNum, line);
    }
  }
  
  file.close();
  
  Serial.printf("Loaded %d historical data points from CSV\n", loadedCount);
  return loadedCount > 0;
}

void checkExistingFermentation() {
  #if !SD_ENABLED
    Serial.println("SD disabled, skipping fermentation file check");
    // Create new file when data arrives
    return;
  #endif
  
  // Mount SD temporarily to check for existing fermentation files
  if (mountSDTemporarily()) {
    // Ensure fermentation directory exists
    if (!SD.exists("/fermentation")) {
      SD.mkdir("/fermentation");
      Serial.println("Created fermentation directory");
    }
    
    File root = SD.open("/fermentation");
    if (!root) {
      Serial.println("Failed to open fermentation directory, creating new file");
      dismountSD();
      createNewFermentationFile();
      return;
    }
    
    int highestNumber = 0;
    bool hasExistingFiles = false;
    
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        hasExistingFiles = true;
        const char* filename = file.name();
        // Parse fermentation_XXX.csv format
        int fileNum = 0;
        if (sscanf(filename, "fermentation_%d.csv", &fileNum) == 1) {
          if (fileNum > highestNumber) {
            highestNumber = fileNum;
          }
        }
      }
      file = root.openNextFile();
    }
    
    root.close();
    
    if (hasExistingFiles && highestNumber > 0) {
      // Auto-continue the highest numbered file
      sprintf(currentFermentationFile, "/fermentation/fermentation_%03d.csv", highestNumber);
      fermentationFileOpen = true;
      Serial.printf("Auto-continuing fermentation file: %s\n", currentFermentationFile);
      
      // Load historical data from the CSV file
      if (loadHistoricalDataFromCSV(currentFermentationFile)) {
        Serial.println("Historical data loaded successfully - views will show previous data");
      } else {
        Serial.println("Failed to load historical data - starting fresh");
      }
    } else {
      // No existing files, create fermentation_001.csv
      Serial.println("No existing fermentation files found, creating fermentation_001.csv");
      createNewFermentationFile();
    }
    
    dismountSD();
  }
}

void createNewFermentationFile() {
  #if !SD_ENABLED
    Serial.println("SD disabled, cannot create fermentation file");
    return;
  #endif
  
  // Mount SD temporarily to create new fermentation file
  if (mountSDTemporarily()) {
    // Ensure fermentation directory exists
    if (!SD.exists("/fermentation")) {
      SD.mkdir("/fermentation");
    }
    
    char filename[64];
    int newFileNumber = 1;
    
    // Parse current file number if exists
    if (strlen(currentFermentationFile) > 0) {
      int currentNum = 0;
      if (sscanf(currentFermentationFile, "/fermentation/fermentation_%d.csv", &currentNum) == 1) {
        newFileNumber = currentNum + 1;
      }
    }
    
    // Find next available number if file already exists
    while (true) {
      snprintf(filename, sizeof(filename), "/fermentation/fermentation_%03d.csv", newFileNumber);
      if (!SD.exists(filename)) {
        break;
      }
      newFileNumber++;
    }
    
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to create new fermentation file");
      dismountSD();
      return;
    }
    
    // Write CSV header
    file.println("uptime_s,angle,density,temperature,battery_voltage,battery_percent");
    file.flush();  // Ensure header is written to SD card
    file.close();
    
    strncpy(currentFermentationFile, filename, sizeof(currentFermentationFile) - 1);
    currentFermentationFile[sizeof(currentFermentationFile) - 1] = '\0';
    fermentationFileOpen = true;
    
    // Reset OG/ABV for new fermentation
    originalGravity = 0.0;
    currentABV = 0.0;
    readingCount = 0;
    ogCaptured = false;
    last3Readings[0] = 0.0;
    last3Readings[1] = 0.0;
    last3Readings[2] = 0.0;
    
    Serial.printf("Created new fermentation file: %s (OG/ABV reset)\n", currentFermentationFile);
    dismountSD();
  }
}

void continueFermentationFile() {
  fermentationFileOpen = true;
  Serial.printf("Continuing fermentation file: %s\n", currentFermentationFile);
}

bool logDataToSD(payload_t data) {
  #if !SD_ENABLED
    return false;
  #endif
  
  if (!fermentationFileOpen || strlen(currentFermentationFile) == 0) {
    return false;
  }
  
  // Mount SD card if not already mounted with improved SPI handling
  // Ensure display CS is HIGH before SD operation
  digitalWrite(DISPLAY_CS, HIGH);
  delayMicroseconds(50); // Increased delay for SPI settling
  
  // Initialize separate SPI for SD card (this was missing!)
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdSPI.setFrequency(4000000); // Set SD SPI to 4MHz for reliability
  delayMicroseconds(50); // Allow SPI to stabilize
  
  // Try SD mount with limited attempts to avoid blocking
  int mountAttempts = 0;
  const int MAX_MOUNT_ATTEMPTS = 2;
  bool sdMounted = false;
  
  while (mountAttempts < MAX_MOUNT_ATTEMPTS && !sdMounted) {
    sdMounted = SD.begin(SD_CS, sdSPI);
    if (!sdMounted) {
      mountAttempts++;
      if (mountAttempts < MAX_MOUNT_ATTEMPTS) {
        delay(100); // Short delay between attempts
      }
    }
  }
  
  if (!sdMounted) {
    // Only log failure occasionally to reduce spam
    static unsigned long lastSDFailureLog = 0;
    if (millis() - lastSDFailureLog > 10000) { // Log every 10 seconds max
      Serial.println("Failed to mount SD card for data logging");
      Serial.printf("SD CS pin: %d, MOSI: %d, MISO: %d, SCK: %d\n", SD_CS, SD_MOSI, SD_MISO, SD_SCK);
      Serial.println("Troubleshooting: Check SD card insertion, wiring, and format (FAT32)");
      lastSDFailureLog = millis();
    }
    
    // Restore SPI bus state after failed mount attempt
    sdSPI.end(); // End separate SPI instance completely
    digitalWrite(SD_CS, HIGH);
    digitalWrite(DISPLAY_CS, HIGH);
    delayMicroseconds(100);
    
    return false;
  }
  
  // Ensure display CS is HIGH before SD operation
  digitalWrite(DISPLAY_CS, HIGH);
  delayMicroseconds(10);
  
  File file = SD.open(currentFermentationFile, FILE_APPEND);
  if (!file) {
    Serial.printf("Failed to open fermentation file '%s' for writing\n", currentFermentationFile);
    
    // Try to create the file if it doesn't exist
    Serial.println("Attempting to create fermentation file...");
    file = SD.open(currentFermentationFile, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to create fermentation file");
      return false;
    }
    
    // Write CSV header for new file
    file.println("uptime,angle,sg,temperature,voltage,battery_percent");
    file.close();
    
    // Try opening again in append mode
    file = SD.open(currentFermentationFile, FILE_APPEND);
    if (!file) {
      Serial.println("Failed to open fermentation file after creation");
      return false;
    }
    Serial.println("Successfully created new fermentation file with header");
  }
  
  // Add timeout protection
  unsigned long startTime = millis();
  const unsigned long SD_TIMEOUT = 5000; // 5 second timeout
  
  // Write CSV data
  uint8_t batteryPercent = calculateBatteryPercentage(data.battery_voltage);
  Serial.printf("DEBUG: Writing to SD - SG:%.4f, Temp:%.2f, Batt:%.2fV\n", 
                data.density, data.temperature, data.battery_voltage);
  int bytesWritten = file.printf("%lu,%.2f,%.4f,%.2f,%.2f,%d\n", 
             data.uptime_s, data.angle, data.density, data.temperature, 
             data.battery_voltage, batteryPercent);
  
  // Check for timeout
  if (millis() - startTime > SD_TIMEOUT) {
    Serial.println("SD write operation timed out");
    file.close();
    SD.end(); // Properly dismount SD card
    sdSPI.end(); // End separate SPI instance completely
    
    // Restore SPI bus state for touch/display
    digitalWrite(SD_CS, HIGH);
    digitalWrite(DISPLAY_CS, HIGH);
    delayMicroseconds(100);
    
    // Reinitialize touch controller SPI state
    tft.getTouch(NULL, NULL); // Dummy touch read to reset touch controller
    
    return false;
  }
  
  file.flush();  // Ensure data is written to SD card
  file.close();
  
  // Dismount SD card to prevent SPI conflicts with touch/display
  SD.end();
  sdSPI.end(); // End separate SPI instance completely
  
  // Restore SPI bus state for touch/display
  digitalWrite(SD_CS, HIGH); // Ensure SD CS is HIGH
  digitalWrite(DISPLAY_CS, HIGH); // Ensure display CS is HIGH
  delayMicroseconds(100); // Allow SPI lines to settle
  
  // Reinitialize touch controller SPI state
  tft.getTouch(NULL, NULL); // Dummy touch read to reset touch controller
  
  if (bytesWritten > 0) {
    Serial.printf("Logged data to SD: SG=%.4f, Temp=%.2f°C, Battery=%.2fV (%d%%)\n", 
                 data.density, data.temperature, data.battery_voltage, batteryPercent);
    return true;
  } else {
    Serial.println("Failed to write data to SD file");
    return false;
  }
}

void drawRebootPrompt() {
  tft.fillScreen(uiColorBackground);
  
  // Title card
  uiCard(MARGIN, TOPBAR_H + MARGIN, UI_W - MARGIN * 2, 60, CARD_RADIUS);
  tft.setTextColor(uiColorAccent);
  tft.setFreeFont(FONT_SIZE_XL);
  tft.setCursor(MARGIN + 60, TOPBAR_H + MARGIN + 30);  // Adjust for FreeFont baseline
  tft.print("Fermentation");
  
  // Question (concise)
  int contentY = TOPBAR_H + MARGIN + 80;
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_MD);
  uiTextCenter(MARGIN, contentY, UI_W - MARGIN * 2, 20, "Continue or start new?", FONT_SIZE_MD, uiColorTextPrimary);
  
  // Continue button
  drawButton(BUTTON_CONTINUE_X, BUTTON_CONTINUE_Y, BUTTON_CONTINUE_W, BUTTON_CONTINUE_H, "CONTINUE", false);
  
  // New button
  drawButton(BUTTON_NEW_X, BUTTON_NEW_Y, BUTTON_NEW_W, BUTTON_NEW_H, "NEW", false);
  
  // Ellipsized file name
  char ellipsized[32];
  uiEllipsize(currentFermentationFile, UI_W - MARGIN * 4, ellipsized, sizeof(ellipsized));
  tft.setTextColor(uiColorTextMuted);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN * 2, 244);  // Adjust for FreeFont baseline
  tft.print(ellipsized);
}

void drawCreateNewDialog() {
  // Draw semi-transparent overlay on current view
  tft.fillRect(20, 80, UI_W - 40, UI_H - 160, uiColorCardBackground);
  
  // Title
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_LG);
  uiTextCenter(20, 100, UI_W - 40, 24, "Create New Fermentation?", FONT_SIZE_LG, uiColorTextPrimary);
  
  // Current file info
  char ellipsized[32];
  uiEllipsize(currentFermentationFile, UI_W - 80, ellipsized, sizeof(ellipsized));
  tft.setTextColor(uiColorTextMuted);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(40, 145);
  tft.print("Current: ");
  tft.print(ellipsized);
  
  // Yes button
  drawButton(BUTTON_DIALOG_YES_X, BUTTON_DIALOG_YES_Y, BUTTON_DIALOG_YES_W, BUTTON_DIALOG_YES_H, "YES", false);
  
  // Back button
  drawButton(BUTTON_DIALOG_BACK_X, BUTTON_DIALOG_BACK_Y, BUTTON_DIALOG_BACK_W, BUTTON_DIALOG_BACK_H, "BACK", false);
}

void logOGToSD() {
  #if !SD_ENABLED
    return;
  #endif
  
  if (!fermentationFileOpen || strlen(currentFermentationFile) == 0) {
    return;
  }
  
  // Ensure display CS is HIGH before SD operation
  digitalWrite(DISPLAY_CS, HIGH);
  delayMicroseconds(10);
  
  File file = SD.open(currentFermentationFile, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open fermentation file for OG logging");
    return;
  }
  
  // Write OG as a comment line in the CSV file
  int bytesWritten = file.printf("# OG_CAPTURED: %.4f at reading #%d\n", originalGravity, readingCount);
  
  file.flush();  // Ensure OG is written to SD card
  file.close();
  
  if (bytesWritten > 0) {
    Serial.printf("OG logged to SD: %.4f at reading #%d\n", originalGravity, readingCount);
  } else {
    Serial.println("Failed to write OG to SD card");
  }
}

void checkOGStability(float currentSG) {
  // Store current reading in the last 3 readings buffer
  last3Readings[readingCount % 3] = currentSG;
  readingCount++;
  
  Serial.printf("OG check: reading #%d, SG=%.4f\n", readingCount, currentSG);
  
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
    
    Serial.printf("Last 3 readings: %.4f, %.4f, %.4f, variance: %.4f\n", r1, r2, r3, variance);
    
    // Check if 3 readings in a row are stable (within threshold)
    if (variance < OG_STABILITY_THRESHOLD) {
      // Calculate average of the 3 stable readings
      originalGravity = (r1 + r2 + r3) / 3.0;
      ogCaptured = true;
      Serial.printf("OG captured as stable: %.4f (average of 3 readings)\n", originalGravity);
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
    Serial.printf("OG captured by fallback (reading #%d): %.4f (average of last 3)\n", readingCount, originalGravity);
    logOGToSD();
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

void showSDErrorIfFailed() {
  // TODO: Implement SD error display if needed
  // This function can be used to show SD card error messages
  // Set sdErrorMessageShown to true when error is displayed
  sdErrorMessageShown = true;
}

// More view - four large option cards with chevrons
void drawMoreView() {
  tft.fillScreen(uiColorBackground);
  
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar("More", true, fermentationFileOpen, battPercent);
  
  int contentY = TOPBAR_H + MARGIN;
  int cardH = (UI_H - TOPBAR_H - NAV_H - MARGIN * 2 - GAP * 3) / 4;
  int cardW = UI_W - MARGIN * 2;
  
  // 1) System
  uiCard(MARGIN, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_LG);
  tft.setCursor(MARGIN + 15, contentY + cardH / 2 - 4);  // Adjust for FreeFont baseline
  tft.print("System >");
  
  // 2) Set OG
  contentY += cardH + GAP;
  uiCard(MARGIN, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(ogCaptured ? uiColorTextMuted : uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_LG);
  tft.setCursor(MARGIN + 15, contentY + cardH / 2 - 4);  // Adjust for FreeFont baseline
  tft.print(ogCaptured ? "Set OG (done)" : "Set OG >");
  
  // 3) Create New Fermentation
  contentY += cardH + GAP;
  uiCard(MARGIN, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorAccent);
  tft.setFreeFont(FONT_SIZE_LG);
  tft.setCursor(MARGIN + 15, contentY + cardH / 2 - 4);  // Adjust for FreeFont baseline
  tft.print("New Fermentation >");
  
  // 4) Reboot
  contentY += cardH + GAP;
  uiCard(MARGIN, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorError);
  tft.setFreeFont(FONT_SIZE_LG);
  tft.setCursor(MARGIN + 15, contentY + cardH / 2 - 4);  // Adjust for FreeFont baseline
  tft.print("Reboot >");
  
  uiDrawBottomNav(TAB_MORE);
}

// Live Details view - structured cards with KV data
void drawLiveDetailsView() {
  tft.fillScreen(uiColorBackground);
  
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar("Details", true, fermentationFileOpen, battPercent);
  
  int contentY = TOPBAR_H + MARGIN;
  int contentW = UI_W - MARGIN * 2;
  
  // Header card: SG value + last update
  int headerH = 50;
  uiCard(MARGIN, contentY, contentW, headerH, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_LG);
  tft.setCursor(MARGIN + 15, contentY + 18);  // Adjust for FreeFont baseline
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    tft.printf("SG: %.3f", latest.density);
    tft.setTextColor(uiColorTextMuted);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN + 15, contentY + 38);  // Adjust for FreeFont baseline
    unsigned long secs = (millis() - lastUpdate) / 1000;
    tft.printf("Last update: %lus", secs);
  } else {
    tft.print("SG: --");
  }
  contentY += headerH + GAP;
  
  // Connection card (half width)
  int halfW = (contentW - GAP) / 2;
  int cardH = 80;
  uiCard(MARGIN, contentY, halfW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorAccent);
  tft.setFreeFont(FONT_SIZE_MD);
  tft.setCursor(MARGIN + 10, contentY + 14);  // Adjust for FreeFont baseline
  tft.print("Connection");
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN + 10, contentY + 34);  // Adjust for FreeFont baseline
  tft.print("ESP-NOW: OK");
  tft.setCursor(MARGIN + 10, contentY + 49);  // Adjust for FreeFont baseline
  tft.print("RSSI: --");
  tft.setCursor(MARGIN + 10, contentY + 64);  // Adjust for FreeFont baseline
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    tft.printf("SEQ: %d", latest.sequence_id);
  } else {
    tft.print("SEQ: --");
  }
  
  // Power card (half width)
  uiCard(MARGIN + halfW + GAP, contentY, halfW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorAccent);
  tft.setFreeFont(FONT_SIZE_MD);
  tft.setCursor(MARGIN + halfW + GAP + 10, contentY + 14);  // Adjust for FreeFont baseline
  tft.print("Power");
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN + halfW + GAP + 10, contentY + 34);  // Adjust for FreeFont baseline
  tft.printf("Battery: %d%%", battPercent);
  tft.setCursor(MARGIN + halfW + GAP + 10, contentY + 49);  // Adjust for FreeFont baseline
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    tft.printf("Voltage: %.2fV", latest.battery_voltage);
  } else {
    tft.print("Voltage: --");
  }
  tft.setCursor(MARGIN + halfW + GAP + 10, contentY + 64);  // Adjust for FreeFont baseline
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    tft.printf("Uptime: %lus", latest.uptime_s);
  } else {
    tft.print("Uptime: --");
  }
  contentY += cardH + GAP;
  
  // Logging card (full width)
  int logH = 70;
  uiCard(MARGIN, contentY, contentW, logH, CARD_RADIUS);
  tft.setTextColor(uiColorAccent);
  tft.setFreeFont(FONT_SIZE_MD);
  tft.setCursor(MARGIN + 10, contentY + 14);  // Adjust for FreeFont baseline
  tft.print("Logging");
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN + 10, contentY + 34);  // Adjust for FreeFont baseline
  tft.printf("SD: %s", fermentationFileOpen ? "OK" : "ERR");
  tft.setCursor(MARGIN + 10, contentY + 49);  // Adjust for FreeFont baseline
  char ellipsized[24];
  uiEllipsize(currentFermentationFile, contentW - 80, ellipsized, sizeof(ellipsized));
  tft.printf("File: %s", ellipsized);
  tft.setCursor(MARGIN + 10, contentY + 64);  // Adjust for FreeFont baseline
  tft.printf("Points: %d", displayDataCount);
  contentY += logH + GAP;
  
  // OG/ABV row (only shown in LIVE_DETAILS_VIEW)
  int ogRowH = 35;
  uiCard(MARGIN, contentY, contentW, ogRowH, CARD_RADIUS);
  tft.setTextColor(uiColorTextMuted);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN + 10, contentY + 14);  // Adjust for FreeFont baseline
  tft.print("OG: ");
  tft.setTextColor(uiColorTextPrimary);
  if (ogCaptured) {
    tft.printf("%.3f", originalGravity);
  } else {
    tft.print("Not set");
  }
  
  // ABV on right side
  tft.setTextColor(uiColorTextMuted);
  tft.setCursor(MARGIN + contentW / 2, contentY + 14);  // Adjust for FreeFont baseline
  tft.print("ABV: ");
  tft.setTextColor(uiColorTextPrimary);
  if (ogCaptured && displayDataCount > 0) {
    tft.printf("%.1f%%", currentABV);
  } else {
    tft.print("—");
  }
  contentY += ogRowH + GAP;
  
  // SET OG button (only if OG not captured)
  if (!ogCaptured && displayDataCount > 0) {
    drawButton(MARGIN, contentY, contentW, 40, "SET OG", false);
  }
  
  uiDrawBottomNav(TAB_LIVE);
  
  // Reset dirty flag after redraw
  detailsDirty = false;
}

// Time-based SPI separation implementation

// Mount SD card temporarily for operations
bool mountSDTemporarily() {
  if (!sdInitialized) {
    // Serial.println("Mounting SD card temporarily...");  // Reduced spam
    
    // Initialize separate SPI for SD card
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    
    // Use the separate SPI instance for SD card
    if (!SD.begin(SD_CS, sdSPI)) {
      Serial.println("SD card mount failed!");
      sdCardPresent = false;
      return false;
    }
    sdInitialized = true;
    sdCardPresent = true;
    // Serial.println("SD card mounted successfully with separate SPI");  // Reduced spam
  }
  return true;
}

// Dismount SD card to restore touch functionality
void dismountSD() {
  if (sdInitialized) {
    SD.end();
    sdSPI.end(); // End the separate SPI instance
    sdInitialized = false;
    // Serial.println("SD card dismounted with separate SPI ended");  // Reduced spam
    
    // Touch system should remain stable (separate SPI buses)
    // Serial.println("Touch system should remain stable (separate SPI buses)");  // Reduced spam
  }
}

// Buffer sensor data to avoid real-time SD operations
void bufferSensorData(payload_t data) {
  if (bufferedCount < MAX_DATA_BUFFER) {
    // Normal case: buffer has space
    dataBuffer[bufferIndex].data = data;
    dataBuffer[bufferIndex].timestamp = millis();
    dataBuffer[bufferIndex].written = false;
    bufferIndex = (bufferIndex + 1) % MAX_DATA_BUFFER;
    bufferedCount++;
    
    if (bufferedCount % 50 == 0) { // Only log every 50th message to reduce spam
      Serial.printf("Buffered sensor data: seq=%d, sg=%.3f, temp=%.1f°C (buffer: %d/%d)\n", 
                    data.sequence_id, data.density, data.temperature, bufferedCount, MAX_DATA_BUFFER);
    }
  } else {
    // Buffer full: implement proper circular buffer overwrite
    static unsigned long lastOverflowWarning = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastOverflowWarning > 10000) { // Warn every 10 seconds
      Serial.printf("WARNING: Sensor data buffer full (%d entries) - overwriting oldest data\n", MAX_DATA_BUFFER);
      Serial.println("Consider increasing SD write frequency or buffer size");
      lastOverflowWarning = currentTime;
    }
    
    // Overwrite oldest data (circular buffer behavior)
    int oldestIndex = bufferIndex; // bufferIndex points to oldest when full
    dataBuffer[oldestIndex].data = data;
    dataBuffer[oldestIndex].timestamp = currentTime;
    dataBuffer[oldestIndex].written = false;
    
    // Move buffer index forward (maintains circular behavior)
    bufferIndex = (bufferIndex + 1) % MAX_DATA_BUFFER;
    // bufferedCount stays at MAX_DATA_BUFFER when full
    
    // Count unwritten entries to track buffer health
    int unwrittenCount = 0;
    for (int i = 0; i < MAX_DATA_BUFFER; i++) {
      if (!dataBuffer[i].written) {
        unwrittenCount++;
      }
    }
    
    if (unwrittenCount > MAX_DATA_BUFFER * 0.8) { // 80% threshold
      Serial.printf("CRITICAL: Buffer health warning - %d/%d entries unwritten\n", 
                    unwrittenCount, MAX_DATA_BUFFER);
    }
  }
}

// Write all buffered sensor data to SD card
void writeBufferedDataToSD() {
  if (bufferedCount == 0) {
    return; // No data to write
  }
  
  static unsigned long lastWriteLog = 0;
  unsigned long currentTime = millis();
  unsigned long operationStartTime = millis();
  const unsigned long MAX_SD_OPERATION_TIME = 2000; // Max 2 seconds for SD operations
  
  // Only log when actually writing data (rate limited)
  if (currentTime - lastWriteLog > 5000) { // Log every 5 seconds max
    Serial.printf("Writing %d sensor data points to SD\n", bufferedCount);
    lastWriteLog = currentTime;
  }
  
  // logDataToSD now handles SD mounting internally, so we don't need mountSDTemporarily()
int writtenCount = 0;
int checkedCount = 0;

// Handle circular buffer properly
// When buffer is full, we need to check all entries, not just bufferedCount
int entriesToCheck = (bufferedCount == MAX_DATA_BUFFER) ? MAX_DATA_BUFFER : bufferedCount;

for (int i = 0; i < entriesToCheck && checkedCount < bufferedCount; i++) {
  // Check if we've exceeded the maximum time for SD operations
  if (millis() - operationStartTime > MAX_SD_OPERATION_TIME) {
    Serial.printf("SD operation timeout after %dms, deferring remaining writes\n", MAX_SD_OPERATION_TIME);
    break;
  }
  
  if (!dataBuffer[i].written && dataBuffer[i].timestamp > 0) {
    if (logDataToSD(dataBuffer[i].data)) {
      dataBuffer[i].written = true;
      writtenCount++;
    }
    checkedCount++;
  }
}

// Only log success when data was actually written
if (writtenCount > 0) {
  Serial.printf("Wrote %d points to SD\n", writtenCount);
}
lastSDWrite = millis();

// Compact buffer: move unwritten entries to the beginning
int newCount = 0;
for (int i = 0; i < entriesToCheck; i++) {
  if (!dataBuffer[i].written && dataBuffer[i].timestamp > 0) {
    // Move unwritten data to front of buffer
    if (newCount != i) {
      dataBuffer[newCount] = dataBuffer[i];
    }
    newCount++;
  }
}

// Update buffer pointers
bufferedCount = newCount;
bufferIndex = newCount; // Reset buffer index to end of valid data

// Debug: Show buffer status (rate limited)
static unsigned long lastDebugLog = 0;
if (currentTime - lastDebugLog > 10000) { // Every 10 seconds
  Serial.printf("Buffer status: wrote %d, remaining %d/%d\n", writtenCount, bufferedCount, MAX_DATA_BUFFER);
  lastDebugLog = currentTime;
}
}

// Check if it's safe to perform SD operations (no recent touch activity)
bool isSafeForSDOperation() {
  // Only perform SD operations if no recent touch AND we haven't tried SD operations too recently
  unsigned long timeSinceLastTouch = millis() - lastTouchTime;
  unsigned long timeSinceLastSDWrite = millis() - lastSDWrite;
  
  return (timeSinceLastTouch > TOUCH_TIMEOUT && 
          timeSinceLastSDWrite > SD_WRITE_INTERVAL && 
          bufferedCount > 0);
}

void loadFloatMacFromEEPROM();
void saveFloatMacToEEPROM();
void startWait(int durationMs);
void checkWaitTimeout();
void stopWait();

void loadFloatMacFromEEPROM() {
  EEPROM.begin(128); // Reserve space for float MAC
  uint32_t magic;
  EEPROM.get(EEPROM_FLOAT_MAC_MAGIC, magic);
  
  if (magic == FLOAT_MAC_MAGIC) {
    EEPROM.get(EEPROM_FLOAT_MAC_ADDR, floatMac);
    floatMacKnown = true;
    Serial.printf("Float MAC loaded from EEPROM: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                  floatMac[0], floatMac[1], floatMac[2], floatMac[3], floatMac[4], floatMac[5]);
    
    // Register float as peer immediately
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, floatMac, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    
    esp_err_t addResult = esp_now_add_peer(&peerInfo);
    if (addResult == ESP_OK) {
      Serial.println("Float unit registered as ESP-NOW peer (from EEPROM)");
    } else if (addResult == ESP_ERR_ESPNOW_EXIST) {
      Serial.println("Float unit already registered as peer");
    } else {
      Serial.printf("Failed to register float as peer: %d\n", addResult);
    }
  } else {
    Serial.println("No valid float MAC found in EEPROM");
  }
}

void saveFloatMacToEEPROM() {
  if (!floatMacKnown) return;
  
  EEPROM.begin(128);
  EEPROM.put(EEPROM_FLOAT_MAC_MAGIC, FLOAT_MAC_MAGIC);
  EEPROM.put(EEPROM_FLOAT_MAC_ADDR, floatMac);
  
  bool success = EEPROM.commit();
  if (success) {
    Serial.println("Float MAC saved to EEPROM");
  } else {
    Serial.println("ERROR: Failed to save float MAC to EEPROM");
  }
}

void startWait(int durationMs) {
  waitStartTime = millis();
  waitDuration = durationMs;
  waitActive = true;
  screenDirty = true;
  Serial.printf("Wait started for %d ms\n", durationMs);
}

void checkWaitTimeout() {
  if (!waitActive) return;
  
  if (millis() - waitStartTime >= waitDuration) {
    stopWait();
    
    // Handle different wait scenarios
    if (calibMode == CALIB_APPLYING) {
      // Apply timeout - show failure message
      calibMode = CALIB_FAILED;
      Serial.println("Apply timeout - calibration failed");
    } else if (calibMode >= CALIB_POINT1 && calibMode <= CALIB_POINT4) {
      // Record timeout - advance to next step
      switch (calibMode) {
        case CALIB_POINT1: calibMode = CALIB_POINT2; break;
        case CALIB_POINT2: calibMode = CALIB_POINT3; break;
        case CALIB_POINT3: calibMode = CALIB_POINT4; break;
        case CALIB_POINT4: calibMode = CALIB_COMPLETE; break;
        default: break;
      }
      Serial.printf("Record timeout - advanced to next step\n");
    }
    
    screenDirty = true;
  }
}

void stopWait() {
  waitActive = false;
  waitDuration = 0;
  Serial.println("Wait stopped");
}