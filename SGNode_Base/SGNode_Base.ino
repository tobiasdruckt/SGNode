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
#include <TFT_eSPI.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <EEPROM.h>
#include "Logo2_Optimized.h"
#include "ui_tokens.h"
#include "ui_components.h"
#include "../SGNode_Shared/sg_protocol.h"
#include "../SGNode_Base/polynomial_calibration.h"

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
  
  // Graph line colors
  uint16_t graphPurple;  // For ABV
  uint16_t graphBlue;    // For SG
  uint16_t graphGreen;   // For Temperature
};

// Light mode theme (default)
Theme lightTheme = {
  .background = rgb565(255, 255, 255),      // White
  .cardBackground = rgb565(248, 248, 248), // Light gray
  .buttonInactive = rgb565(224, 224, 224), // Lighter gray
  .primary = rgb565(11, 31, 42),           // Navy
  .primaryText = rgb565(255, 255, 255),   // White
  .accent = rgb565(235, 235, 235),          // Gold
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
  .gridLine = rgb565(221, 221, 221),      // Light grid
  .graphPurple = rgb565(156, 39, 176),    // Purple for ABV
  .graphBlue = rgb565(33, 150, 243),      // Blue for SG
  .graphGreen = rgb565(76, 175, 80)      // Green for Temperature
};

// Dark mode theme
Theme darkTheme = {
  .background       = rgb565(11, 31, 42),     // Logo-Farbe
  .cardBackground   = rgb565(22, 30, 56),
  .buttonInactive   = rgb565(38, 48, 74),

  .primary          = rgb565(110, 131, 142),    // 
  .primaryText      = rgb565(11, 31, 42),

  .accent           = rgb565(22, 30, 56),
  .accentText       = rgb565(11, 31, 42),

  .gold             = rgb565(110, 131, 142),

  .textPrimary      = rgb565(245, 247, 250),
  .textSecondary    = rgb565(170, 180, 195),
  .textMuted        = rgb565(110, 120, 140),

  .success          = rgb565(46, 204, 113),
  .warning          = rgb565(255, 193, 7),
  .error            = rgb565(255, 82, 82),
  .info             = rgb565(64, 169, 255),

  .border           = rgb565(45, 55, 80),
  .gridLine         = rgb565(35, 45, 65),

  .graphPurple      = rgb565(180, 100, 255),
  .graphBlue        = rgb565(64, 169, 255),
  .graphGreen       = rgb565(46, 204, 113)
};

// Current theme and mode
Theme* currentTheme = &darkTheme;
bool darkMode = true;

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
#define MAX_DATA_POINTS 150
payload_t displayDataBuffer[MAX_DATA_POINTS];
int displayDataIndex = 0;
int displayDataCount = 0;
int totalCSVDataLines = 0;  // Total data points in CSV file

// Separate buffer for discharge rate calculation (25 recent points only)
payload_t dischargeRateBuffer[25];
int dischargeRateBufferCount = 0;

// Cursor state for graph view
int cursorIndex = 0;           // Index of selected data point
float cursorPosition = 1.0;    // 0.0 to 1.0 (0% to 100% of X-axis)

// Graph data loading mode
enum GraphLoadMode { LOAD_6H, LOAD_24H, LOAD_ALL };
GraphLoadMode currentLoadMode = LOAD_6H;
GraphLoadMode batteryLoadMode = LOAD_6H;

// RSSI tracking for ESP-NOW signal strength
int lastRSSI = 0;
bool rssiAvailable = false;

// Graph points for polynomial display
struct GraphPoint {
  int x;
  int y;
  bool valid;
};
GraphPoint graphPoints[91];

// ABV calculation variables
float originalGravity = 0.0;  // OG captured when stable
float currentABV = 0.0;       // Calculated ABV
int readingCount = 0;         // Number of readings since fermentation started
float last3Readings[3] = {0.0, 0.0, 0.0}; // Last 3 SG readings for stability check
bool ogCaptured = false;      // Whether OG has been captured
#define OG_STABILITY_THRESHOLD 0.005 // SG variance threshold for stability
#define OG_MAX_READINGS 10     // Force capture at this reading if not stable

// Battery tracking variables
enum ChargingState {
  CHARGING_UNKNOWN,
  CHARGING,
  DISCHARGING
};
ChargingState chargingState = CHARGING_UNKNOWN;
float lastBatteryVoltage = 0.0;
unsigned long lastBatteryCheck = 0;

// Display objects - TFT_eSPI includes built-in XPT2046 touch support
TFT_eSPI tft = TFT_eSPI();

// UI state
enum ViewMode {
  LIVE_VIEW,
  GRAPH_VIEW,
  CALIBRATION_VIEW,
  BATTERY_VIEW,
  MORE_VIEW,
  LIVE_DETAILS_VIEW,
  POLY_GRAPH_VIEW,
  TEMP_GRAPH_VIEW,
  ANGLE_GRAPH_VIEW,
  ABV_GRAPH_VIEW
};

ViewMode currentMode = LIVE_VIEW;

// Graph metric types for parameterized graph drawing
enum GraphMetric { METRIC_DENSITY, METRIC_TEMPERATURE, METRIC_ANGLE, METRIC_ABV };

// Calibration state
enum CalibMode {
  CALIB_IDLE,
  CALIB_INSTRUCTIONS,
  CALIB_OFFSET,
  CALIB_POINT1,
  CALIB_POINT2,
  CALIB_POINT3,
  CALIB_POINT4,
  CALIB_COMPLETE,
  CALIB_APPLYING,
  CALIB_FAILED,
  CALIB_SKIP_WARNING
};

CalibMode calibMode = CALIB_IDLE;
float calibAngles[4] = {0.0, 0.0, 0.0, 0.0}; // Store angles for 4 calibration points
float calibSG[4] = {1.000, 1.040, 1.080, 1.120}; // Target SG values for 4 points
int sugarAmounts[4] = {0, 80, 80, 80}; // Sugar to add at each step (grams) - for 2L water
float calibOffset = 0.0; // Store sensor offset angle
bool offsetCalibrated = false; // Track if offset calibration is complete
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

// Calibration buttons (step-by-step wizard) - reorganized for consistent layout
#define BUTTON_CALIB_START_X 110
#define BUTTON_CALIB_START_Y 180
#define BUTTON_CALIB_START_W 100
#define BUTTON_CALIB_START_H 40

#define BUTTON_CALIB_NEXT_X 220
#define BUTTON_CALIB_NEXT_Y 180
#define BUTTON_CALIB_NEXT_W 100
#define BUTTON_CALIB_NEXT_H 40

#define BUTTON_CALIB_RECORD_X 110
#define BUTTON_CALIB_RECORD_Y 180
#define BUTTON_CALIB_RECORD_W 100
#define BUTTON_CALIB_RECORD_H 40

#define BUTTON_CALIB_APPLY_X 220
#define BUTTON_CALIB_APPLY_Y 194
#define BUTTON_CALIB_APPLY_W 100
#define BUTTON_CALIB_APPLY_H 40

#define BUTTON_CALIB_EXIT_X 220
#define BUTTON_CALIB_EXIT_Y 220
#define BUTTON_CALIB_EXIT_W 100
#define BUTTON_CALIB_EXIT_H 25

// Offset calibration buttons - repositioned for Step 1/5 layout
#define BUTTON_CALIB_OFFSET_X 220  // Lower right
#define BUTTON_CALIB_OFFSET_Y 194  // 6px higher
#define BUTTON_CALIB_OFFSET_W 100
#define BUTTON_CALIB_OFFSET_H 40

// Record button - moved to same position as Calibrate button
#define BUTTON_CALIB_RECORD_X 220
#define BUTTON_CALIB_RECORD_Y 194
#define BUTTON_CALIB_RECORD_W 100
#define BUTTON_CALIB_RECORD_H 40

// Exit button for Step 1/5 - lower left
#define BUTTON_CALIB_EXIT_OFFSET_X 80   // Lower left
#define BUTTON_CALIB_EXIT_OFFSET_Y 200
#define BUTTON_CALIB_EXIT_OFFSET_W 100
#define BUTTON_CALIB_EXIT_OFFSET_H 40

// Skip SG Calibration buttons
#define BUTTON_CALIB_SKIP_X 80
#define BUTTON_CALIB_SKIP_Y 194
#define BUTTON_CALIB_SKIP_W 100
#define BUTTON_CALIB_SKIP_H 40

// Skip confirmation buttons - repositioned to avoid conflicts
#define BUTTON_CALIB_SKIP_YES_X 80
#define BUTTON_CALIB_SKIP_YES_Y 180
#define BUTTON_CALIB_SKIP_YES_W 100
#define BUTTON_CALIB_SKIP_YES_H 40

#define BUTTON_CALIB_SKIP_NO_X 220
#define BUTTON_CALIB_SKIP_NO_Y 180
#define BUTTON_CALIB_SKIP_NO_W 100
#define BUTTON_CALIB_SKIP_NO_H 40

// Function prototypes
void initESPNow();
void initDisplay();
void initTouch();
void initSDCard();
void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
void onCalibrationResponse(const uint8_t *mac, const uint8_t *incomingData, int len);
void onCalibrationCommandFromFloat(const uint8_t *mac, const uint8_t *incomingData, int len);
void onCalibrationCoefficients(const uint8_t *mac, const uint8_t *incomingData, int len);
void drawLiveView();
void drawGraphView();
void drawCalibrationView();
void drawBatteryView();
void drawMoreView();
void drawLiveDetailsView();
void drawPolynomialGraph();
void drawTempGraphView();
void drawAngleGraphView();
void drawAbvGraphView();
void calculateGraphPoints();
void drawGraphAxes();
void drawGraphCurve();
void checkTouch();
float calculateGravity(float tilt);

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
bool loadHistoricalDataFromCSV(const char* filename, 
                               GraphLoadMode mode = LOAD_6H,
                               int maxPoints = MAX_DATA_POINTS,
                               int samplingStep = 1);
void checkOGStability(float currentSG);
void logOGToSD();
float calculateABV(float og, float currentSG);
uint8_t calculateBatteryPercentage(float voltage);
void updateChargingState(float currentVoltage);
float calculateDischargeRateLinearRegression();
void showSDErrorIfFailed();

// Graph cursor helper functions
void formatTimeFromDataPoint(int index, char* buf, int bufSize);
void updateCursorPosition();
void drawCursorValueDisplay(int x, int y, bool showRight);

// Graph button handler functions
void handleGraphButton6h();
void handleGraphButton24h();
void handleGraphButtonAll();
void handleGraphButtonLeft();
void handleGraphButtonRight();

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
  initCalibration();
  
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
        case BATTERY_VIEW:
          drawBatteryView();
          break;
        case MORE_VIEW:
          drawMoreView();
          break;
        case POLY_GRAPH_VIEW:
          drawPolynomialGraph();
          break;
        case LIVE_DETAILS_VIEW:
          drawLiveDetailsView();
          break;
        case TEMP_GRAPH_VIEW:
          drawTempGraphView();
          break;
        case ANGLE_GRAPH_VIEW:
          drawAngleGraphView();
          break;
        case ABV_GRAPH_VIEW:
          drawAbvGraphView();
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
        case BATTERY_VIEW:
          title = "Battery";
          break;
        case MORE_VIEW:
          title = "More";
          break;
        case POLY_GRAPH_VIEW:
          title = "Polynomial";
          break;
        case LIVE_DETAILS_VIEW:
          title = "Details";
          break;
        case TEMP_GRAPH_VIEW:
          title = "Temp";
          break;
        case ANGLE_GRAPH_VIEW:
          title = "Angle";
          break;
        case ABV_GRAPH_VIEW:
          title = "ABV";
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
    // Capture RSSI from received packet
    if (recv_info->rx_ctrl != NULL) {
      lastRSSI = recv_info->rx_ctrl->rssi;
      rssiAvailable = true;
    }
    
    // Check message type first for 26-byte packets to distinguish between sensor data and coefficients
    if (len == sizeof(calib_response_t)) { // 42 bytes - Calibration response
      onCalibrationResponse(recv_info->src_addr, data, len);
    } else if (len == sizeof(calib_coeffs_t)) { // 26 bytes - Calibration coefficients
      // Check first byte to distinguish from sensor data
      if (len >= 1 && data[0] == 3) { // response_type = 3 for coefficients
        onCalibrationCoefficients(recv_info->src_addr, data, len);
      } else {
        onDataReceived(recv_info->src_addr, data, len); // Regular sensor data
      }
    } else if (len == sizeof(calib_command_t)) { // 6 bytes - Calibration command from float
      onCalibrationCommandFromFloat(recv_info->src_addr, data, len);
    } else if (len == sizeof(payload_t)) { // 26 bytes - Regular sensor data (fallback)
      onDataReceived(recv_info->src_addr, data, len);
    } else {
      Serial.printf("Unknown packet size: %d bytes\n", len);
    }
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
    
    // Update charging state based on voltage trends
    updateChargingState(receivedData.battery_voltage);
    
    Serial.printf("Received: seq=%d, angle=%.2f°, density=%.3f, temp=%.1f°C, battery=%.2fV (%d%%) [%s]\n", 
                  receivedData.sequence_id, receivedData.angle, receivedData.density, 
                  receivedData.temperature, receivedData.battery_voltage, batteryPercent,
                  chargingState == CHARGING ? "CHARGING" : 
                  chargingState == DISCHARGING ? "DISCHARGING" : "UNKNOWN");
    
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
    // Also update BATTERY_VIEW if active
    if (currentMode == BATTERY_VIEW) {
      screenDirty = true;  // Trigger full battery view redraw
    }
    // Also update new graph views if active - auto-update with new data
    if (currentMode == TEMP_GRAPH_VIEW || currentMode == ANGLE_GRAPH_VIEW || currentMode == ABV_GRAPH_VIEW) {
      screenDirty = true;  // Trigger full graph redraw
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
  int tileY = contentY + heroH + GAP - 6;  // Lifted by 6px
  int tileW = (UI_W - MARGIN * 2 - GAP * 2) / 3;  // 3 tiles with gaps
  int tileH = contentH - heroH - GAP;
  
  uiTile(MARGIN, tileY, tileW, tileH, 0, "Temp", "--", "°C", false);
  uiTile(MARGIN + tileW + GAP, tileY, tileW, tileH, 0, "Angle", "--", "deg", false);
  uiTile(MARGIN + tileW * 2 + GAP * 2, tileY, tileW, tileH, 0, "ABV", "Set OG", "%", true);
  
  // Bottom navigation
  uiDrawBottomNav(TAB_LIVE);
  
  // Reset dirty flags
  heroDirty = true;
  tileTempDirty = true;
  tileAngleDirty = true;
  tileAbvDirty = true;
  staticElementsDrawn = false;  // Force full redraw after tile position change
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
  int tileY = contentY + heroH + GAP - 6;  // Lifted by 6px
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
    uiTile(MARGIN + tileW + GAP, tileY, tileW, tileH, 0, "Angle", buffer, "deg", false);
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
  
  // Define button area
  #define GRAPH_BUTTON_H 40
  #define GRAPH_BUTTON_Y (UI_H - NAV_H - GRAPH_BUTTON_H - GAP)
  
  int contentY = TOPBAR_H;
  int contentH = GRAPH_BUTTON_Y - contentY - GAP;
  
  if (displayDataCount < 3) {
    uiCard(MARGIN, contentY + contentH / 2 - 40, UI_W - MARGIN * 2, 80, CARD_RADIUS);
    uiTextCenter(MARGIN, contentY + contentH / 2 - 10, UI_W - MARGIN * 2, 20, "No graph data", FONT_SIZE_MD, uiColorTextMuted);
    uiTextCenter(MARGIN, contentY + contentH / 2 + 10, UI_W - MARGIN * 2, 20, "Collect a few points first", FONT_SIZE_SM, uiColorTextMuted);
    uiDrawBottomNav(TAB_GRAPH);
    return;
  }
  
  // Initialize cursor to last data point (100%) only if cursor is at invalid position
  if (cursorIndex < 0 || cursorIndex >= displayDataCount) {
    cursorPosition = 1.0;
    cursorIndex = displayDataCount - 1;
  }
  
  // Draw graph card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, contentH, CARD_RADIUS);
  drawGraph();
  
  // Draw 5 buttons in a single row below graph
  int buttonW = (UI_W - MARGIN * 2 - GAP * 4) / 5;
  int buttonX = MARGIN;
  
  // 6h button
  drawButton(buttonX, GRAPH_BUTTON_Y, buttonW, GRAPH_BUTTON_H, "6h", currentLoadMode == LOAD_6H);
  buttonX += buttonW + GAP;
  
  // 24h button
  drawButton(buttonX, GRAPH_BUTTON_Y, buttonW, GRAPH_BUTTON_H, "24h", currentLoadMode == LOAD_24H);
  buttonX += buttonW + GAP;
  
  // All button
  drawButton(buttonX, GRAPH_BUTTON_Y, buttonW, GRAPH_BUTTON_H, "All", currentLoadMode == LOAD_ALL);
  buttonX += buttonW + GAP;
  
  // < button
  drawButton(buttonX, GRAPH_BUTTON_Y, buttonW, GRAPH_BUTTON_H, "<", false);
  buttonX += buttonW + GAP;
  
  // > button
  drawButton(buttonX, GRAPH_BUTTON_Y, buttonW, GRAPH_BUTTON_H, ">", false);
  
  uiDrawBottomNav(TAB_GRAPH);
}

void drawGrid(int graphX, int graphY, int graphW, int graphH, float minDensity, float maxDensity) {
  // Grid and Y-axis labels removed as requested
}

void drawGraph() {
  if (displayDataCount < 2) return;
  
  // Graph area inside the card with reduced margins (no axis labels)
  int axisMarginLeft = 10;
  int axisMarginBottom = 10;
  int graphX = MARGIN + 10 + axisMarginLeft;
  int graphY = TOPBAR_H + 10;
  int graphW = UI_W - MARGIN * 2 - 20 - axisMarginLeft;
  int graphH = GRAPH_BUTTON_Y - graphY - GAP - axisMarginBottom;
  
  // Find min/max for each metric
  float minDensity = 1.0, maxDensity = 1.1;
  float minTemp = 15.0, maxTemp = 30.0;
  float minAbv = 0.0, maxAbv = 10.0;
  
  for (int i = 0; i < displayDataCount; i++) {
    payload_t data = displayDataBuffer[(displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    if (data.density < minDensity) minDensity = data.density;
    if (data.density > maxDensity) maxDensity = data.density;
    if (data.temperature < minTemp) minTemp = data.temperature;
    if (data.temperature > maxTemp) maxTemp = data.temperature;
    if (ogCaptured) {
      float abv = calculateABV(originalGravity, data.density);
      if (abv < minAbv) minAbv = abv;
      if (abv > maxAbv) maxAbv = abv;
    }
  }
  
  // Add padding
  minDensity -= 0.002;
  maxDensity += 0.002;
  minTemp -= 1.0;
  maxTemp += 1.0;
  minAbv -= 0.5;
  maxAbv += 0.5;
  
  // Draw grid with axis labels (use SG as primary Y-axis)
  drawGrid(graphX, graphY, graphW, graphH, minDensity, maxDensity);
  
  // Calculate time range for X-axis labels based on total CSV data points (600s interval)
  // For LOAD_ALL mode, use totalCSVDataLines from CSV, not loaded points (capped at MAX_DATA_POINTS)
  int totalPointsForTime = (currentLoadMode == LOAD_ALL) ? totalCSVDataLines : displayDataCount;
  
  if (totalPointsForTime >= 2) {
    uint32_t timeSpan = (totalPointsForTime - 1) * 600;
    
    // Draw X-axis label
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_XS);
    tft.setCursor(graphX + graphW / 2 - 15, graphY + graphH + axisMarginBottom - 5);
    tft.print("Time");
    
    // Draw X-axis time labels (start at 0, end at total time span)
    char timeBuf[32];
    if (timeSpan < 3600) {
      // Show minutes
      snprintf(timeBuf, sizeof(timeBuf), "0min");
      tft.setCursor(graphX - 5, graphY + graphH + 15);
      tft.print(timeBuf);
      snprintf(timeBuf, sizeof(timeBuf), "%dmin", (int)(timeSpan / 60));
      tft.setCursor(graphX + graphW - 30, graphY + graphH + 15);
      tft.print(timeBuf);
    } else if (timeSpan < 86400) {
      // Show hours
      snprintf(timeBuf, sizeof(timeBuf), "0h");
      tft.setCursor(graphX - 5, graphY + graphH + 15);
      tft.print(timeBuf);
      snprintf(timeBuf, sizeof(timeBuf), "%.1fh", timeSpan / 3600.0);
      tft.setCursor(graphX + graphW - 30, graphY + graphH + 15);
      tft.print(timeBuf);
    } else {
      // Show days
      snprintf(timeBuf, sizeof(timeBuf), "0d");
      tft.setCursor(graphX - 5, graphY + graphH + 15);
      tft.print(timeBuf);
      snprintf(timeBuf, sizeof(timeBuf), "%dd %dh", (int)(timeSpan / 86400), (int)((timeSpan % 86400) / 3600));
      tft.setCursor(graphX + graphW - 40, graphY + graphH + 15);
      tft.print(timeBuf);
    }
  }

  // Prevent division by zero
  float densityRange = maxDensity - minDensity;
  if (densityRange == 0) densityRange = 0.001;
  float tempRange = maxTemp - minTemp;
  if (tempRange == 0) tempRange = 0.001;
  float abvRange = maxAbv - minAbv;
  if (abvRange == 0) abvRange = 0.001;

  // Draw three polylines: SG (blue), Temperature (green), ABV (purple)
  int prevX_SG = -1, prevY_SG = -1;
  int prevX_Temp = -1, prevY_Temp = -1;
  int prevX_Abv = -1, prevY_Abv = -1;

  for (int i = 0; i < displayDataCount; i++) {
    payload_t data = displayDataBuffer[(displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS];

    int x = graphX + (i * graphW / (displayDataCount - 1));
    
    // SG line (blue)
    int y_SG = graphY + graphH - ((data.density - minDensity) / densityRange * graphH);
    if (prevX_SG >= 0 && prevY_SG >= 0) {
      tft.drawLine(prevX_SG, prevY_SG, x, y_SG, currentTheme->graphBlue);
    }
    prevX_SG = x;
    prevY_SG = y_SG;
    
    // Temperature line (green)
    int y_Temp = graphY + graphH - ((data.temperature - minTemp) / tempRange * graphH);
    if (prevX_Temp >= 0 && prevY_Temp >= 0) {
      tft.drawLine(prevX_Temp, prevY_Temp, x, y_Temp, currentTheme->graphGreen);
    }
    prevX_Temp = x;
    prevY_Temp = y_Temp;
    
    // ABV line (purple) - only if OG is set
    if (ogCaptured) {
      float abv = calculateABV(originalGravity, data.density);
      int y_Abv = graphY + graphH - ((abv - minAbv) / abvRange * graphH);
      if (prevX_Abv >= 0 && prevY_Abv >= 0) {
        tft.drawLine(prevX_Abv, prevY_Abv, x, y_Abv, currentTheme->graphPurple);
      }
      prevX_Abv = x;
      prevY_Abv = y_Abv;
    }
  }
  
  // Draw cursor
  updateCursorPosition();
  int cursorX = graphX + (int)(cursorPosition * graphW);
  
  // Get data at cursor position
  if (cursorIndex >= 0 && cursorIndex < displayDataCount) {
    payload_t cursorData = displayDataBuffer[(displayDataIndex - displayDataCount + cursorIndex + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    
    // Calculate Y positions for each metric at cursor
    int y_SG = graphY + graphH - ((cursorData.density - minDensity) / densityRange * graphH);
    int y_Temp = graphY + graphH - ((cursorData.temperature - minTemp) / tempRange * graphH);
    
    // Draw cursor dot on SG line (blue)
    tft.fillCircle(cursorX, y_SG, 4, currentTheme->graphBlue);
    
    // Draw cursor dot on Temperature line (green)
    tft.fillCircle(cursorX, y_Temp, 4, currentTheme->graphGreen);
    
    // Draw cursor dot on ABV line (purple) if OG is captured
    if (ogCaptured) {
      float abv = calculateABV(originalGravity, cursorData.density);
      int y_Abv = graphY + graphH - ((abv - minAbv) / abvRange * graphH);
      tft.fillCircle(cursorX, y_Abv, 4, currentTheme->graphPurple);
    }
  }
  
  // Draw vertical cursor line
  tft.drawLine(cursorX, graphY, cursorX, graphY + graphH, currentTheme->accent);
  
  // Draw cursor indicator (triangle at top)
  int cursorIndicatorY = graphY - 5;
  tft.fillTriangle(cursorX, cursorIndicatorY, cursorX - 5, cursorIndicatorY - 5, cursorX + 5, cursorIndicatorY - 5, currentTheme->accent);
  
  // Draw cursor value display
  bool showRight = cursorPosition < 0.44;
  int valueDisplayY = graphY + graphH / 2 - 30;
  drawCursorValueDisplay(cursorX, valueDisplayY, showRight);
}

// Parameterized graph drawing function for different metrics
void drawGraphForMetric(GraphMetric metric) {
  if (displayDataCount < 2) return;
  
  // Graph area inside the card with margins for axis labels
  int axisMarginLeft = 45;
  int axisMarginBottom = 25;
  int graphX = MARGIN + 10 + axisMarginLeft;
  int graphY = TOPBAR_H + 10;
  int graphW = UI_W - MARGIN * 2 - 20 - axisMarginLeft;
  int graphH = UI_H - TOPBAR_H - NAV_H - 20 - axisMarginBottom;
  
  // Find min/max values for the specific metric
  float minValue = 0, maxValue = 0;
  const char* yAxisLabel = "";
  float padding = 0;
  
  // Initialize based on metric type
  switch (metric) {
    case METRIC_TEMPERATURE:
      minValue = 10.0, maxValue = 20.0;
      yAxisLabel = "°C";
      padding = 1.0;
      break;
    case METRIC_ANGLE:
      minValue = 0.0, maxValue = 45.0;
      yAxisLabel = "deg";
      padding = 5.0;
      break;
    case METRIC_ABV:
      minValue = 0.0, maxValue = 10.0;
      yAxisLabel = "%";
      padding = 1.0;
      break;
    case METRIC_DENSITY:
    default:
      minValue = 1.0, maxValue = 1.1;
      yAxisLabel = "SG";
      padding = 0.002;
      break;
  }
  
  // Calculate actual min/max from data
  for (int i = 0; i < displayDataCount; i++) {
    payload_t data = displayDataBuffer[(displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    float value = 0;
    
    switch (metric) {
      case METRIC_TEMPERATURE:
        value = data.temperature;
        break;
      case METRIC_ANGLE:
        value = data.angle;
        break;
      case METRIC_ABV:
        if (ogCaptured) {
          value = calculateABV(originalGravity, data.density);
        }
        break;
      case METRIC_DENSITY:
      default:
        value = data.density;
        break;
    }
    
    if (value < minValue) minValue = value;
    if (value > maxValue) maxValue = value;
  }
  
  // Add padding
  minValue -= padding;
  maxValue += padding;
  
  // Draw grid with axis labels
  drawGrid(graphX, graphY, graphW, graphH, minValue, maxValue);
  
  // Update Y-axis label
  tft.setTextColor(uiColorTextMuted);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(graphX - 25, graphY + 10);
  tft.print(yAxisLabel);
  
  // Update Y-axis min/max values
  char buf[16];
  snprintf(buf, sizeof(buf), metric == METRIC_DENSITY ? "%.3f" : "%.1f", maxValue);
  tft.setCursor(graphX - 35, graphY + 5);
  tft.print(buf);
  
  snprintf(buf, sizeof(buf), metric == METRIC_DENSITY ? "%.3f" : "%.1f", minValue);
  tft.setCursor(graphX - 35, graphY + graphH);
  tft.print(buf);
  
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
  float valueRange = maxValue - minValue;
  if (valueRange == 0) valueRange = 0.001;

  // Draw gold polyline
  int prevX = -1, prevY = -1;

  for (int i = 0; i < displayDataCount; i++) {
    payload_t data = displayDataBuffer[(displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    float value = 0;
    
    switch (metric) {
      case METRIC_TEMPERATURE:
        value = data.temperature;
        break;
      case METRIC_ANGLE:
        value = data.angle;
        break;
      case METRIC_ABV:
        if (ogCaptured) {
          value = calculateABV(originalGravity, data.density);
        } else {
          value = 0;
        }
        break;
      case METRIC_DENSITY:
      default:
        value = data.density;
        break;
    }

    int x = graphX + (i * graphW / (displayDataCount - 1));
    int y = graphY + graphH - ((value - minValue) / valueRange * graphH);

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
  contentY += 14;
  
  // Main content card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, contentH - 50, CARD_RADIUS);
  
  if (calibMode == CALIB_IDLE) {
    uiTextCenter(MARGIN, contentY + 20, UI_W - MARGIN * 2, 24, "Calibration", FONT_SIZE_LG, uiColorTextPrimary);
    drawButton(BUTTON_CALIB_START_X, BUTTON_CALIB_START_Y, BUTTON_CALIB_START_W, BUTTON_CALIB_START_H, "START", false);
    
  } else if (calibMode == CALIB_INSTRUCTIONS) {
    // Calculate proper text positions using consistent layout
    tft.setTextColor(uiColorAccent);
    tft.setFreeFont(FONT_SIZE_MD);
    int titleY = contentY + 10;
    uiTextCenter(MARGIN, titleY, UI_W - MARGIN * 2, 24, "Calibration Steps", FONT_SIZE_SM_BOLD, COLOR_BLUE);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 6; // Consistent line spacing
    int contentStartY = titleY + 35; // Start content below title
    
    int bullet1Y = contentStartY;
    int bullet2Y = bullet1Y + lineHeight;
    
    tft.setCursor(MARGIN + 15, bullet1Y);
    tft.print("1. Prepare bowl (3L+ capacity)");
    tft.setCursor(MARGIN + 15, bullet2Y);
    tft.print("2. Add 2L water, place float");
    drawButton(BUTTON_CALIB_NEXT_X, BUTTON_CALIB_NEXT_Y, BUTTON_CALIB_NEXT_W, BUTTON_CALIB_NEXT_H, "NEXT", false);
    
  } else if (calibMode == CALIB_OFFSET) {
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, "Step 1/5 - Sensor Offset Calibration", FONT_SIZE_SM_BOLD, COLOR_BLUE);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 40 + tft.fontHeight(); // Proper baseline
    int bullet2Y = bullet1Y + lineHeight;
    int bullet3Y = bullet2Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.print("• Place float on flat, level surface");
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("• Ensure device is completely still");
    tft.setCursor(MARGIN + 10, bullet3Y);
    tft.print("• Press Calibrate Offset");
    drawButton(BUTTON_CALIB_OFFSET_X, BUTTON_CALIB_OFFSET_Y, BUTTON_CALIB_OFFSET_W, BUTTON_CALIB_OFFSET_H, "Calibrate", offsetCalibrated);
    
  } else if (calibMode == CALIB_SKIP_WARNING) {
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, "Skip SG Calibration?", FONT_SIZE_MD, uiColorAccent);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int textY = contentY + 40 + tft.fontHeight(); // Proper baseline
    
    tft.setCursor(MARGIN + 10, textY);
    tft.print("Save sensor offset only and");
    textY += lineHeight;
    tft.setCursor(MARGIN + 10, textY);
    tft.print("return to Live View?");
    
    drawButton(BUTTON_CALIB_SKIP_YES_X, BUTTON_CALIB_SKIP_YES_Y, BUTTON_CALIB_SKIP_YES_W, BUTTON_CALIB_SKIP_YES_H, "Yes", false);
    drawButton(BUTTON_CALIB_SKIP_NO_X, BUTTON_CALIB_SKIP_NO_Y, BUTTON_CALIB_SKIP_NO_W, BUTTON_CALIB_SKIP_NO_H, "No", false);
    
  } else if (calibMode == CALIB_POINT1) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Step 2/5 - Calibration Point 1 (SG 1.000)");
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, buf, FONT_SIZE_SM_BOLD, COLOR_BLUE);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 40 + tft.fontHeight(); // Proper baseline
    int bullet2Y = bullet1Y + lineHeight;
    int bullet3Y = bullet2Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.print("• Use bowl with 3L+ capacity");
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("• Fill with 2.0L water");
    tft.setCursor(MARGIN + 10, bullet3Y);
    tft.print("• Wait, then Record");
    drawButton(BUTTON_CALIB_SKIP_X, BUTTON_CALIB_SKIP_Y, BUTTON_CALIB_SKIP_W, BUTTON_CALIB_SKIP_H, "Skip SG", false);
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "Record", calibAngles[0] > 0);
    
  } else if (calibMode == CALIB_POINT2) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Step 3/5 - Calibration Point 2 (SG 1.040)");
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, buf, FONT_SIZE_SM_BOLD, COLOR_BLUE);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 40 + tft.fontHeight(); // Proper baseline
    int bullet2Y = bullet1Y + lineHeight;
    int bullet3Y = bullet2Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.printf("• Add %dg sugar", sugarAmounts[1]);
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("• Stir until fully dissolved");
    tft.setCursor(MARGIN + 10, bullet3Y);
    tft.print("• Wait, then Record");
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "Record", calibAngles[1] > 0);
    
  } else if (calibMode == CALIB_POINT3) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Step 4/5 - Calibration Point 3 (SG 1.080)");
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, buf, FONT_SIZE_SM_BOLD, COLOR_BLUE);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 40 + tft.fontHeight(); // Proper baseline
    int bullet2Y = bullet1Y + lineHeight;
    int bullet3Y = bullet2Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.printf("• Add %dg sugar", sugarAmounts[2]);
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("• Stir until fully dissolved");
    tft.setCursor(MARGIN + 10, bullet3Y);
    tft.print("• Wait, then Record");
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "Record", calibAngles[2] > 0);
    
  } else if (calibMode == CALIB_POINT4) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Step 5/5 - Calibration Point 4 (SG 1.120)");
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, buf, FONT_SIZE_SM_BOLD, COLOR_BLUE);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 40 + tft.fontHeight(); // Proper baseline
    int bullet2Y = bullet1Y + lineHeight;
    int bullet3Y = bullet2Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.printf("• Add %dg sugar", sugarAmounts[3]);
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("• Stir until fully dissolved");
    tft.setCursor(MARGIN + 10, bullet3Y);
    tft.print("• Wait, then Record");
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "Record", calibAngles[3] > 0);
    
  } else if (calibMode == CALIB_COMPLETE) {
    uiTextCenter(MARGIN, contentY + 15, UI_W - MARGIN * 2, 20, "4 Points Recorded", FONT_SIZE_MD, uiColorTextPrimary);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int summaryY = contentY + 40;
    
    char buf[64];
    tft.setCursor(MARGIN + 10, summaryY);
    tft.print("Summary:");
    summaryY += lineHeight;
    
    snprintf(buf, sizeof(buf), "P1 (SG 1.000): %.1f°", calibAngles[0]);
    tft.setCursor(MARGIN + 10, summaryY);
    tft.print(buf);
    summaryY += lineHeight;
    
    snprintf(buf, sizeof(buf), "P2 (SG 1.040): %.1f°", calibAngles[1]);
    tft.setCursor(MARGIN + 10, summaryY);
    tft.print(buf);
    summaryY += lineHeight;
    
    snprintf(buf, sizeof(buf), "P3 (SG 1.080): %.1f°", calibAngles[2]);
    tft.setCursor(MARGIN + 10, summaryY);
    tft.print(buf);
    summaryY += lineHeight;
    
    snprintf(buf, sizeof(buf), "P4 (SG 1.120): %.1f°", calibAngles[3]);
    tft.setCursor(MARGIN + 10, summaryY);
    tft.print(buf);
    
    drawButton(BUTTON_CALIB_APPLY_X, BUTTON_CALIB_APPLY_Y, BUTTON_CALIB_APPLY_W, BUTTON_CALIB_APPLY_H, "Confirm", false);
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
  
  // Exit button (only shown in offset calibration state)
  if (calibMode == CALIB_OFFSET) {
    drawButton(80, 194, BUTTON_CALIB_OFFSET_W, BUTTON_CALIB_OFFSET_H, "EXIT", false);
  }
  
  uiDrawBottomNav(TAB_BATTERY);
  
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

void drawBatteryView() {
  tft.fillScreen(uiColorBackground);
  
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar("Battery", true, fermentationFileOpen, battPercent);
  
  int contentY = TOPBAR_H;
  int contentH = UI_H - TOPBAR_H - NAV_H;
  
  if (displayDataCount < 3) {
    uiCard(MARGIN, contentY + contentH / 2 - 40, UI_W - MARGIN * 2, 80, CARD_RADIUS);
    uiTextCenter(MARGIN, contentY + contentH / 2 - 10, UI_W - MARGIN * 2, 20, "No battery data", FONT_SIZE_MD, uiColorTextMuted);
    uiTextCenter(MARGIN, contentY + contentH / 2 + 10, UI_W - MARGIN * 2, 20, "Collect a few points first", FONT_SIZE_SM, uiColorTextMuted);
    uiDrawBottomNav(TAB_BATTERY);
    return;
  }
  
  // Left 2/3: Battery voltage graph
  int graphW = (UI_W - MARGIN * 2 - GAP) * 2 / 3;
  int graphX = MARGIN;
  int graphH = contentH;
  int graphY = contentY - 8;
  
  // Define button area for battery view
  #define BATTERY_BUTTON_H 40
  #define BATTERY_BUTTON_Y (UI_H - NAV_H - BATTERY_BUTTON_H - GAP)
  
  // Adjust graph height to accommodate buttons
  int graphContentH = BATTERY_BUTTON_Y - graphY - GAP;
  
  uiCard(graphX, graphY, graphW, graphH, CARD_RADIUS);
  
  // Draw battery sparkline graph
  // Load historical battery data from CSV based on batteryLoadMode (similar to graph view)
  if (fermentationFileOpen && strlen(currentFermentationFile) > 0) {
    // Load battery voltage data from CSV based on time range
    int loadMultiplier = 1;
    if (batteryLoadMode == LOAD_6H) {
      loadMultiplier = 1;
    } else if (batteryLoadMode == LOAD_24H) {
      loadMultiplier = 2;
    } else {
      loadMultiplier = 1;
    }
    
    // Load data with appropriate time range (use same logic as graph view)
    if (mountSDTemporarily()) {
      loadHistoricalDataFromCSV(currentFermentationFile, batteryLoadMode, MAX_DATA_POINTS, loadMultiplier);
      dismountSD();
    }
  }
  
  // Extract battery voltage data from loaded displayDataBuffer
  float batteryData[MAX_DATA_POINTS];
  int batteryCount = 0;
  int pointsToShow = min(displayDataCount, MAX_DATA_POINTS);
  for (int i = 0; i < pointsToShow; i++) {
    int idx = (displayDataIndex - pointsToShow + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    batteryData[i] = displayDataBuffer[idx].battery_voltage;
    batteryCount++;
  }
  
  // Draw sparkline with margins and reduced height for buttons
  int sparkX = graphX + 50;  // Increased left margin for y-axis labels
  int sparkY = graphY + 40;
  int sparkW = graphW - 70;  // Reduced width for y-axis labels
  int sparkH = graphContentH - 80;  // Use available space below title
  
  uiDrawSparkline(sparkX, sparkY, sparkW, sparkH, batteryData, batteryCount);
  
  // Graph title
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(graphX + 20, graphY + 25);
  tft.print("Battery Voltage (V)");
  
  // Draw voltage range labels on y-axis
  if (batteryCount >= 2) {
    float minVoltage = batteryData[0];
    float maxVoltage = batteryData[0];
    for (int i = 1; i < batteryCount; i++) {
      if (batteryData[i] < minVoltage) minVoltage = batteryData[i];
      if (batteryData[i] > maxVoltage) maxVoltage = batteryData[i];
    }
    
    // Add small padding
    minVoltage -= 0.05;
    maxVoltage += 0.05;
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_XS);
    
    // Draw max voltage label
    char voltageBuf[16];
    snprintf(voltageBuf, sizeof(voltageBuf), "%.2f", maxVoltage);
    tft.setCursor(graphX + 15, sparkY + 5);
    tft.print(voltageBuf);
    
    // Draw min voltage label
    snprintf(voltageBuf, sizeof(voltageBuf), "%.2f", minVoltage);
    tft.setCursor(graphX + 15, sparkY + sparkH - 5);
    tft.print(voltageBuf);
  }
  
  // Draw 3 time range buttons below sparkline
  int buttonW = (graphW - MARGIN * 2 - GAP * 2) / 3;
  int buttonX = graphX + MARGIN;
  
  // 6h button
  drawButton(buttonX, BATTERY_BUTTON_Y, buttonW, BATTERY_BUTTON_H, "6h", batteryLoadMode == LOAD_6H);
  buttonX += buttonW + GAP;
  
  // 24h button
  drawButton(buttonX, BATTERY_BUTTON_Y, buttonW, BATTERY_BUTTON_H, "24h", batteryLoadMode == LOAD_24H);
  buttonX += buttonW + GAP;
  
  // All button
  drawButton(buttonX, BATTERY_BUTTON_Y, buttonW, BATTERY_BUTTON_H, "All", batteryLoadMode == LOAD_ALL);
  
  // Right 1/3: Three info cards
  int cardX = graphX + graphW + GAP;
  int cardW = UI_W - cardX - MARGIN;
  int cardH = (contentH - GAP * 2) / 3;
  
  // Voltage card
  uiCard(cardX, graphY - 4, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(cardX + 15, graphY - 4 + 20);
  tft.print("Voltage");
  
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    tft.setTextColor(uiColorTextPrimary);
    tft.setFreeFont(FONT_SIZE_MD);
    char voltageBuf[16];
    snprintf(voltageBuf, sizeof(voltageBuf), "%.2fV", latest.battery_voltage);
    tft.setCursor(cardX + 15, graphY + cardH / 2 + 14);
    tft.print(voltageBuf);
  }
  
  // Percentage card
  uiCard(cardX, graphY + cardH + GAP - 4, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(cardX + 15, graphY + cardH + GAP - 4 + 20);
  tft.print("Percentage");
  
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    uint8_t percent = calculateBatteryPercentage(latest.battery_voltage);
    tft.setTextColor(uiColorTextPrimary);
    tft.setFreeFont(FONT_SIZE_MD);
    char percentBuf[16];
    snprintf(percentBuf, sizeof(percentBuf), "%d%%", percent);
    tft.setCursor(cardX + 15, graphY + cardH + GAP - 4 + cardH / 2 + 14);
    tft.print(percentBuf);
  }
  
  // Status card (charging state + uptime)
  uiCard(cardX, graphY + (cardH + GAP) * 2, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(cardX + 15, graphY + (cardH + GAP) * 2 + 20);
  tft.print("Status");
  
  // Show charging state or uptime estimation
  if (chargingState == CHARGING) {
    tft.setTextColor(uiColorSuccess);
    tft.setFreeFont(FONT_SIZE_MD);
    tft.setCursor(cardX + 15, graphY + (cardH + GAP) * 2 - 4 + cardH / 2 + 14);
    tft.print("Charging");
  } else if (chargingState == DISCHARGING && displayDataCount >= 20) {
    // Calculate discharge rate using linear regression over last 20 readings
    float dischargeRate = calculateDischargeRateLinearRegression(); // Volts per second
    
    Serial.printf("Battery view: dischargeRate=%.6f, threshold=0.00000004\n", dischargeRate);
    
    if (dischargeRate >= 0.00000004) { // Only estimate if discharging significantly (adjusted for 30-day operation)
      payload_t lastReading = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
      float lastVoltage = lastReading.battery_voltage;
      float remainingVoltage = lastVoltage - 3.0; // Voltage until 3.0V cutoff
      float estimatedSeconds = remainingVoltage / dischargeRate;
      int estimatedHours = (int)(estimatedSeconds / 3600);
      
      tft.setTextColor(uiColorTextPrimary);
      tft.setFreeFont(FONT_SIZE_MD);
      char uptimeBuf[16];
      if (estimatedHours < 24) {
        snprintf(uptimeBuf, sizeof(uptimeBuf), "%dh left", estimatedHours);
      } else {
        snprintf(uptimeBuf, sizeof(uptimeBuf), "%dd left", estimatedHours / 24);
      }
      tft.setCursor(cardX + 15, graphY + (cardH + GAP) * 2 + cardH / 2 + 14);
      tft.print(uptimeBuf);
    } else {
      tft.setTextColor(uiColorTextPrimary);
      tft.setFreeFont(FONT_SIZE_MD);
      tft.setCursor(cardX + 15, graphY + (cardH + GAP) * 2 - 4 + cardH / 2 + 14);
      tft.print("Stable");
    }
  } else {
    // Unknown charging state or not enough data
    tft.setTextColor(uiColorTextMuted);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(cardX + 15, graphY + (cardH + GAP) * 2 - 4 + cardH / 2 + 14);
    if (chargingState == CHARGING_UNKNOWN) {
      tft.print("Detecting...");
    } else {
      tft.print("Calculating...");
    }
  }
  
  uiDrawBottomNav(TAB_BATTERY);
}

void drawTempGraphView() {
  tft.fillScreen(uiColorBackground);
  
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar("Temp", true, fermentationFileOpen, battPercent);
  
  int contentY = TOPBAR_H;
  int contentH = UI_H - contentY - NAV_H;
  
  if (displayDataCount < 3) {
    uiCard(MARGIN, contentY + contentH / 2 - 40, UI_W - MARGIN * 2, 80, CARD_RADIUS);
    uiTextCenter(MARGIN, contentY + contentH / 2 - 10, UI_W - MARGIN * 2, 20, "No graph data", FONT_SIZE_MD, uiColorTextMuted);
    uiTextCenter(MARGIN, contentY + contentH / 2 + 10, UI_W - MARGIN * 2, 20, "Collect a few points first", FONT_SIZE_SM, uiColorTextMuted);
    uiDrawBottomNav(TAB_LIVE);
    return;
  }
  
  // Draw graph card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, contentH, CARD_RADIUS);
  drawGraphForMetric(METRIC_TEMPERATURE);
  
  uiDrawBottomNav(TAB_LIVE);
}

void drawAngleGraphView() {
  tft.fillScreen(uiColorBackground);
  
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar("Angle", true, fermentationFileOpen, battPercent);
  
  int contentY = TOPBAR_H;
  int contentH = UI_H - contentY - NAV_H;
  
  if (displayDataCount < 3) {
    uiCard(MARGIN, contentY + contentH / 2 - 40, UI_W - MARGIN * 2, 80, CARD_RADIUS);
    uiTextCenter(MARGIN, contentY + contentH / 2 - 10, UI_W - MARGIN * 2, 20, "No graph data", FONT_SIZE_MD, uiColorTextMuted);
    uiTextCenter(MARGIN, contentY + contentH / 2 + 10, UI_W - MARGIN * 2, 20, "Collect a few points first", FONT_SIZE_SM, uiColorTextMuted);
    uiDrawBottomNav(TAB_LIVE);
    return;
  }
  
  // Draw graph card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, contentH, CARD_RADIUS);
  drawGraphForMetric(METRIC_ANGLE);
  
  uiDrawBottomNav(TAB_LIVE);
}

void drawAbvGraphView() {
  tft.fillScreen(uiColorBackground);
  
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar("ABV", true, fermentationFileOpen, battPercent);
  
  int contentY = TOPBAR_H;
  int contentH = UI_H - contentY - NAV_H;
  
  if (!ogCaptured) {
    uiCard(MARGIN, contentY + contentH / 2 - 40, UI_W - MARGIN * 2, 80, CARD_RADIUS);
    uiTextCenter(MARGIN, contentY + contentH / 2 - 10, UI_W - MARGIN * 2, 20, "OG not set", FONT_SIZE_MD, uiColorTextMuted);
    uiTextCenter(MARGIN, contentY + contentH / 2 + 10, UI_W - MARGIN * 2, 20, "Set OG first", FONT_SIZE_SM, uiColorTextMuted);
    uiDrawBottomNav(TAB_LIVE);
    return;
  }
  
  if (displayDataCount < 3) {
    uiCard(MARGIN, contentY + contentH / 2 - 40, UI_W - MARGIN * 2, 80, CARD_RADIUS);
    uiTextCenter(MARGIN, contentY + contentH / 2 - 10, UI_W - MARGIN * 2, 20, "No graph data", FONT_SIZE_MD, uiColorTextMuted);
    uiTextCenter(MARGIN, contentY + contentH / 2 + 10, UI_W - MARGIN * 2, 20, "Collect a few points first", FONT_SIZE_SM, uiColorTextMuted);
    uiDrawBottomNav(TAB_LIVE);
    return;
  }
  
  // Draw graph card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, contentH, CARD_RADIUS);
  drawGraphForMetric(METRIC_ABV);
  
  uiDrawBottomNav(TAB_LIVE);
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
  
  // Center text - proper centering for FreeFont
  tft.setFreeFont(FONT_SIZE_SM);
  int16_t w1 = tft.textWidth(label); // Get actual text width
  int16_t h1 = tft.fontHeight(); // Get actual font height
  tft.setCursor(x + (w - w1) / 2, y + (h - h1) / 2 + h1);  // Proper centering with baseline
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
      // Map tab index to ViewMode for comparison
      ViewMode tabMode = LIVE_VIEW;
      switch (tab) {
        case TAB_LIVE:
          tabMode = LIVE_VIEW;
          break;
        case TAB_GRAPH:
          tabMode = GRAPH_VIEW;
          break;
        case TAB_BATTERY:
          tabMode = BATTERY_VIEW;
          break;
        case TAB_MORE:
          tabMode = MORE_VIEW;
          break;
      }
      
      if (tabMode != currentMode) {
        switch (tab) {
          case TAB_LIVE:
            currentMode = LIVE_VIEW;
            staticElementsDrawn = false;  // Force full redraw
            screenDirty = true;
            Serial.println("Switched to Live View");
            break;
          case TAB_GRAPH:
            currentMode = GRAPH_VIEW;
            // Auto-load historical data when entering Graph View
            if (fermentationFileOpen && strlen(currentFermentationFile) > 0) {
              loadHistoricalDataFromCSV(currentFermentationFile, currentLoadMode, MAX_DATA_POINTS, 1);
              cursorPosition = 1.0;
              cursorIndex = displayDataCount - 1;
            }
            screenDirty = true;
            Serial.println("Switched to Graph View");
            break;
          case TAB_BATTERY:
            currentMode = BATTERY_VIEW;
            screenDirty = true;
            Serial.println("Switched to Battery View");
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
    
    // Handle Graph view button touches
    if (currentMode == GRAPH_VIEW) {
      int buttonW = (UI_W - MARGIN * 2 - GAP * 4) / 5;
      int buttonX = MARGIN;
      int buttonY = GRAPH_BUTTON_Y;
      int buttonH = GRAPH_BUTTON_H;
      
      // 6h button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        handleGraphButton6h();
        delay(50);
        return;
      }
      buttonX += buttonW + GAP;
      
      // 24h button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        handleGraphButton24h();
        delay(50);
        return;
      }
      buttonX += buttonW + GAP;
      
      // All button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        handleGraphButtonAll();
        delay(50);
        return;
      }
      buttonX += buttonW + GAP;
      
      // < button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        handleGraphButtonLeft();
        delay(50);
        return;
      }
      buttonX += buttonW + GAP;
      
      // > button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        handleGraphButtonRight();
        delay(50);
        return;
      }
    }
    
    // Handle Battery view button touches
    if (currentMode == BATTERY_VIEW) {
      int contentY = TOPBAR_H;
      int contentH = UI_H - TOPBAR_H - NAV_H;
      int graphW = (UI_W - MARGIN * 2 - GAP) * 2 / 3;
      int graphX = MARGIN;
      
      #define BATTERY_BUTTON_H 40
      #define BATTERY_BUTTON_Y (UI_H - NAV_H - BATTERY_BUTTON_H - GAP)
      
      int buttonW = (graphW - MARGIN * 2 - GAP * 2) / 3;
      int buttonX = graphX + MARGIN;
      int buttonY = BATTERY_BUTTON_Y;
      int buttonH = BATTERY_BUTTON_H;
      
      // 6h button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        batteryLoadMode = LOAD_6H;
        screenDirty = true;
        Serial.println("Battery view: 6h selected");
        delay(50);
        return;
      }
      buttonX += buttonW + GAP;
      
      // 24h button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        batteryLoadMode = LOAD_24H;
        screenDirty = true;
        Serial.println("Battery view: 24h selected");
        delay(50);
        return;
      }
      buttonX += buttonW + GAP;
      
      // All button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        batteryLoadMode = LOAD_ALL;
        screenDirty = true;
        Serial.println("Battery view: All selected");
        delay(50);
        return;
      }
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
      
      // Calibration card - opens Polynomial Graph
      contentY += cardH + GAP;
      if (touchX >= MARGIN && touchX <= MARGIN + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        currentMode = POLY_GRAPH_VIEW;
        screenDirty = true;
        Serial.println("Switched to Polynomial Graph");
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
    
    // Handle Live view tile touches for graph views
    if (currentMode == LIVE_VIEW && displayDataCount > 0) {
      int contentY = calibrationModeActive ? TOPBAR_H + 14 : TOPBAR_H;
      int contentH = UI_H - contentY - NAV_H;
      int heroH = contentH / 2;
      int tileY = contentY + heroH + GAP - 6;  // Lifted by 6px
      int tileW = (UI_W - MARGIN * 2 - GAP * 2) / 3;  // 3 tiles
      int tileH = contentH - heroH - GAP;
      
      // Temp tile (first tile)
      int tempTileX = MARGIN;
      if (touchX >= tempTileX && touchX <= tempTileX + tileW &&
          touchY >= tileY && touchY <= tileY + tileH) {
        currentMode = TEMP_GRAPH_VIEW;
        screenDirty = true;
        staticElementsDrawn = false;
        Serial.println("Switched to Temp Graph View");
        delay(50);
        return;
      }
      
      // Angle tile (second tile)
      int angleTileX = MARGIN + tileW + GAP;
      if (touchX >= angleTileX && touchX <= angleTileX + tileW &&
          touchY >= tileY && touchY <= tileY + tileH) {
        currentMode = ANGLE_GRAPH_VIEW;
        screenDirty = true;
        staticElementsDrawn = false;
        Serial.println("Switched to Angle Graph View");
        delay(50);
        return;
      }
      
      // ABV tile (third tile)
      int abvTileX = MARGIN + tileW * 2 + GAP * 2;
      if (touchX >= abvTileX && touchX <= abvTileX + tileW &&
          touchY >= tileY && touchY <= tileY + tileH) {
        if (ogCaptured) {
          // OG is set, show ABV graph
          currentMode = ABV_GRAPH_VIEW;
          screenDirty = true;
          staticElementsDrawn = false;
          Serial.println("Switched to ABV Graph View");
          delay(50);
          return;
        } else {
          // OG not set, set OG
          payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
          originalGravity = latest.density;
          ogCaptured = true;
          currentABV = calculateABV(originalGravity, latest.density);
          screenDirty = true;
          Serial.printf("Set OG to: %.3f\n", originalGravity);
          delay(50);
          return;
        }
      }
    }
    
    // Handle new graph view touches to return to LIVE_VIEW
    if (currentMode == TEMP_GRAPH_VIEW || currentMode == ANGLE_GRAPH_VIEW || currentMode == ABV_GRAPH_VIEW) {
      // Tap anywhere to return to LIVE_VIEW
      currentMode = LIVE_VIEW;
      screenDirty = true;
      staticElementsDrawn = false;
      Serial.println("Returned to Live View");
      delay(50);
      return;
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
      
      // Next button (instructions to offset calibration)
      if (calibMode == CALIB_INSTRUCTIONS &&
          touchX >= BUTTON_CALIB_NEXT_X && touchX <= BUTTON_CALIB_NEXT_X + BUTTON_CALIB_NEXT_W &&
          touchY >= BUTTON_CALIB_NEXT_Y && touchY <= BUTTON_CALIB_NEXT_Y + BUTTON_CALIB_NEXT_H) {
        calibMode = CALIB_OFFSET;
        screenDirty = true;
        Serial.println("Moved to Offset Calibration");
      }
      
      // Calibrate Offset button
      if (calibMode == CALIB_OFFSET &&
          touchX >= BUTTON_CALIB_OFFSET_X && touchX <= BUTTON_CALIB_OFFSET_X + BUTTON_CALIB_OFFSET_W &&
          touchY >= BUTTON_CALIB_OFFSET_Y && touchY <= BUTTON_CALIB_OFFSET_Y + BUTTON_CALIB_OFFSET_H) {
        sendCalibrationCommand(5, 0.0); // Command 5 for offset calibration
        startWait(2000); // 2 second wait for offset calibration
        Serial.println("CALIBRATE OFFSET - started 2s wait");
      }
      
      // Skip SG Calibration button (only in Point 1)
      if (calibMode == CALIB_POINT1 &&
          touchX >= BUTTON_CALIB_SKIP_X && touchX <= BUTTON_CALIB_SKIP_X + BUTTON_CALIB_SKIP_W &&
          touchY >= BUTTON_CALIB_SKIP_Y && touchY <= BUTTON_CALIB_SKIP_Y + BUTTON_CALIB_SKIP_H) {
        calibMode = CALIB_SKIP_WARNING;
        screenDirty = true;
        Serial.println("Show skip SG calibration warning");
      }
      
      // Skip confirmation buttons
      if (calibMode == CALIB_SKIP_WARNING) {
        // Yes button - skip SG calibration
        if (touchX >= BUTTON_CALIB_SKIP_YES_X && touchX <= BUTTON_CALIB_SKIP_YES_X + BUTTON_CALIB_SKIP_YES_W &&
            touchY >= BUTTON_CALIB_SKIP_YES_Y && touchY <= BUTTON_CALIB_SKIP_YES_Y + BUTTON_CALIB_SKIP_YES_H) {
          // Send exit calibration command to float unit
          calib_command_t exitCmd;
          exitCmd.command = 7; // New command for EXIT_CALIBRATION
          exitCmd.target_sg = 0.0;
          exitCmd.request_id = 254; // Special ID for exit
          
          esp_err_t result;
          if (floatMacKnown) {
            result = esp_now_send(floatMac, (uint8_t*)&exitCmd, sizeof(exitCmd));
            Serial.printf("Sent exit calibration command to float: %d\n", result);
          }
          
          // Save offset only and exit to Live View
          currentMode = LIVE_VIEW;
          calibMode = CALIB_IDLE;
          calibrationModeActive = false;
          stopWait(); // Stop any active wait
          screenDirty = true;
          staticElementsDrawn = false; // Force complete redraw to clear artifacts
          Serial.println("SKIP SG CALIBRATION - saved offset only, returned to Live View");
        }
        
        // No button - continue with SG calibration
        if (touchX >= BUTTON_CALIB_SKIP_NO_X && touchX <= BUTTON_CALIB_SKIP_NO_X + BUTTON_CALIB_SKIP_NO_W &&
            touchY >= BUTTON_CALIB_SKIP_NO_Y && touchY <= BUTTON_CALIB_SKIP_NO_Y + BUTTON_CALIB_SKIP_NO_H) {
          calibMode = CALIB_POINT1;
          screenDirty = true;
          Serial.println("Continue with SG calibration");
        }
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
      
      // Exit button - only available in Step 1/5 (CALIB_OFFSET)
      if (calibMode == CALIB_OFFSET &&
          touchX >= 80 && touchX <= 80 + BUTTON_CALIB_OFFSET_W &&
          touchY >= 194 && touchY <= 194 + BUTTON_CALIB_OFFSET_H) {
        // Send exit calibration command to float unit
        calib_command_t exitCmd;
        exitCmd.command = 7; // EXIT_CALIBRATION
        exitCmd.target_sg = 0.0;
        exitCmd.request_id = 254; // Special ID for exit
        
        esp_err_t result;
        if (floatMacKnown) {
          result = esp_now_send(floatMac, (uint8_t*)&exitCmd, sizeof(exitCmd));
          Serial.printf("Sent exit calibration command to float: %d\n", result);
        }
        
        currentMode = LIVE_VIEW;
        calibMode = CALIB_IDLE;
        calibrationModeActive = false;
        stopWait(); // Stop any active wait
        // Reset calibration data
        for (int i = 0; i < 4; i++) calibAngles[i] = 0.0;
        screenDirty = true;
        staticElementsDrawn = false; // Force complete redraw to clear artifacts
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
    // Handle offset calibration response (request_id=5)
    if (calibResp.request_id == 5 && calibMode == CALIB_OFFSET) {
      Serial.println("Offset calibration completed");
      stopWait(); // Stop the 2s wait
      calibOffset = calibResp.angle; // Store offset angle
      offsetCalibrated = true;
      calibMode = CALIB_POINT1; // Move to first SG calibration point
      screenDirty = true;
      Serial.printf("Stored offset angle: %.2f°, moved to Point 1\n", calibOffset);
    }
    // Handle apply response (request_id=4) - polynomial calculation complete
    else if (calibResp.request_id == 4 && calibMode == CALIB_APPLYING) {
      Serial.println("Calibration apply completed - coefficients calculated on float unit");
      
      stopWait(); // Stop the 3s wait
      calibMode = CALIB_IDLE;
      currentMode = LIVE_VIEW; // Exit calibration view
      calibrationModeActive = false;
      screenDirty = true;
      staticElementsDrawn = false; // Force full redraw
      Serial.println("Switched to Live View after calibration completion");
    }
    // Handle exit calibration response (request_id=254) - float unit exiting calibration
    else if (calibResp.request_id == 254) {
      Serial.println("Float unit confirmed exit from calibration mode");
      // Ensure screen is clean if we're already in live view
      if (currentMode == LIVE_VIEW) {
        screenDirty = true;
        staticElementsDrawn = false; // Force full redraw to clear any remaining artifacts
      }
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
void onCalibrationCoefficients(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(calib_coeffs_t)) {
    Serial.printf("Invalid coefficients packet size: %d bytes (expected %d)\n", len, sizeof(calib_coeffs_t));
    return;
  }
  
  calib_coeffs_t* coeffs = (calib_coeffs_t*)incomingData;
  
  // Store received coefficients
  calibCoeffs.coeff3 = coeffs->coeff3;
  calibCoeffs.coeff2 = coeffs->coeff2;
  calibCoeffs.coeff1 = coeffs->coeff1;
  calibCoeffs.coeff0 = coeffs->coeff0;
  calibCoeffs.isValid = true;
  
  // Store normalization parameters
  normOffset = coeffs->norm_offset;
  normScale = coeffs->norm_scale;
  
  // Save to EEPROM for persistence
  saveCalibrationCoefficients();
  
  Serial.println("Received calibration coefficients from float unit:");
  Serial.printf("GRAVITY = %.12e * norm(Tilt)^3 + %.12e * norm(Tilt)^2 + %.12e * norm(Tilt) + %.12e\n",
                calibCoeffs.coeff3, calibCoeffs.coeff2, calibCoeffs.coeff1, calibCoeffs.coeff0);
  Serial.printf("Normalization: norm(Tilt) = (Tilt - %.2f) / %.2f\n", normOffset, normScale);
  Serial.println("Coefficients saved to EEPROM for polynomial graph");
  
  // Test polynomial behavior to detect non-monotonic issues
  testPolynomialRange();
}

void onCalibrationCommandFromFloat(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(calib_command_t)) {
    Serial.printf("Invalid calibration command size from float: %d bytes (expected %d)\n", len, sizeof(calib_command_t));
    return;
  }
  
  calib_command_t cmd;
  memcpy(&cmd, incomingData, len);
  
  Serial.printf("Received calibration command from float: %d, target_sg=%.3f, request_id=%d\n", 
                cmd.command, cmd.target_sg, cmd.request_id);
  
  if (cmd.command == 6 && cmd.request_id == 255) {
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
    
    // Switch to calibration view and start with offset calibration
    currentMode = CALIBRATION_VIEW;
    calibMode = CALIB_OFFSET;
    screenDirty = true;
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

bool loadHistoricalDataFromCSV(const char* filename, 
                               GraphLoadMode mode,
                               int maxPoints,
                               int samplingStep) {
  #if !SD_ENABLED
    return false;
  #endif
  
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.printf("Failed to open CSV file for reading: %s\n", filename);
    return false;
  }
  
  Serial.printf("Loading historical data from: %s (mode=%d, maxPoints=%d, step=%d)\n", 
                filename, mode, maxPoints, samplingStep);
  
  // First pass: count total data lines
  int totalDataLines = 0;
  char line[128];
  
  file.seek(0);
  
  // Count data lines (skip header)
  while (file.available()) {
    int bytesRead = file.readBytesUntil('\n', line, sizeof(line) - 1);
    line[bytesRead] = '\0';
    
    if (file.position() == bytesRead + 1 || line[0] == '#') {
      continue;
    }
    
    if (strlen(line) > 0) {
      totalDataLines++;
    }
  }
  
  Serial.printf("Found %d total data lines in CSV\n", totalDataLines);
  totalCSVDataLines = totalDataLines;
  
  // Calculate loading parameters based on mode using data point count (600s interval)
  int linesToSkip = 0;
  int linesToLoad = maxPoints;
  int actualSamplingStep = samplingStep;
  
  switch (mode) {
    case LOAD_6H:
      // Load last 6 hours (6 hours * 3600s / 600s = 36 points)
      linesToLoad = min(36, totalDataLines);
      linesToSkip = max(0, totalDataLines - linesToLoad);
      actualSamplingStep = 1;
      Serial.printf("LOAD_6H: Loading last %d points (6 hours at 600s interval)\n", linesToLoad);
      break;
      
    case LOAD_24H:
      // Load last 24 hours (24 hours * 3600s / 600s = 144 points)
      linesToLoad = min(144, totalDataLines);
      linesToSkip = max(0, totalDataLines - linesToLoad);
      actualSamplingStep = 1;
      Serial.printf("LOAD_24H: Loading last %d points (24 hours at 600s interval)\n", linesToLoad);
      break;
      
    case LOAD_ALL:
      // Load all data with sampling to fit in maxPoints
      linesToSkip = 0;
      actualSamplingStep = max(1, totalDataLines / maxPoints);
      linesToLoad = totalDataLines;
      Serial.printf("LOAD_ALL: Loading all %d points with step=%d\n", linesToLoad, actualSamplingStep);
      break;
  }
  
  // Reset display data buffer
  displayDataIndex = 0;
  displayDataCount = 0;
  
  // Second pass: load data with line skipping and sampling
  file.seek(0);
  int lineNum = 0;
  int loadedCount = 0;
  int skippedCount = 0;
  int sampleCounter = 0;
  
  while (file.available() && loadedCount < linesToLoad) {
    int bytesRead = file.readBytesUntil('\n', line, sizeof(line) - 1);
    line[bytesRead] = '\0';
    lineNum++;
    
    // Skip header line and comment lines
    if (lineNum == 1 || line[0] == '#') {
      continue;
    }
    
    // Skip old data lines to get to recent data
    if (skippedCount < linesToSkip) {
      skippedCount++;
      continue;
    }
    
    // Apply sampling step
    if (sampleCounter % actualSamplingStep != 0) {
      sampleCounter++;
      continue;
    }
    sampleCounter++;
    
    // Parse CSV line: uptime_s,angle,density,temperature,battery_voltage,battery_percent
    payload_t data = {0};
    uint8_t battery_percent = 0;
    int parsed = sscanf(line, "%lu,%f,%f,%f,%f,%hhu",
                       &data.uptime_s,
                       &data.angle,
                       &data.density,
                       &data.temperature,
                       &data.battery_voltage,
                       &battery_percent);
    
    if (parsed == 6) {
      displayDataBuffer[displayDataIndex] = data;
      displayDataIndex = (displayDataIndex + 1) % MAX_DATA_POINTS;
      if (displayDataCount < MAX_DATA_POINTS) {
        displayDataCount++;
      }
      loadedCount++;
      
      uint8_t battPercent = calculateBatteryPercentage(data.battery_voltage);
      
      if (!ogCaptured && data.density > 1.000) {
        originalGravity = data.density;
        ogCaptured = true;
        Serial.printf("Loaded OG from CSV: %.4f\n", originalGravity);
      }
      
      if (loadedCount <= 3 || loadedCount > linesToLoad - 3) {
        Serial.printf("Loaded point %d: SG=%.4f, Temp=%.1f°C, Batt=%.2fV (%d%%)\n",
                     loadedCount, data.density, data.temperature, data.battery_voltage, battPercent);
      }
    } else if (strlen(line) > 0) {
      Serial.printf("Warning: Failed to parse line %d: %s\n", lineNum, line);
    }
  }
  
  file.close();
  
  Serial.printf("Loaded %d data points from CSV\n", loadedCount);
  
  // Initialize charging state based on historical data
  if (loadedCount >= 2) {
    payload_t secondLastReading = displayDataBuffer[(displayDataIndex - 2 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    payload_t lastReading = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    
    float voltageChange = lastReading.battery_voltage - secondLastReading.battery_voltage;
    
    if (voltageChange > 0.02) {
      chargingState = CHARGING;
      Serial.printf("Initial charging state: CHARGING (voltage increased by %.3fV)\n", voltageChange);
    } else if (voltageChange < -0.02) {
      chargingState = DISCHARGING;
      Serial.printf("Initial charging state: DISCHARGING (voltage decreased by %.3fV)\n", voltageChange);
    } else {
      chargingState = DISCHARGING;
      Serial.printf("Initial charging state: DISCHARGING (voltage stable, change: %.3fV)\n", voltageChange);
    }
    
    lastBatteryVoltage = lastReading.battery_voltage;
    lastBatteryCheck = millis();
  } else {
    Serial.println("Not enough data to determine initial charging state");
  }
  
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
      if (loadHistoricalDataFromCSV(currentFermentationFile, LOAD_6H, MAX_DATA_POINTS, 1)) {
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

void updateChargingState(float currentVoltage) {
  // Only update charging state if we have previous data and enough time has passed
  if (lastBatteryVoltage > 0 && (millis() - lastBatteryCheck > 30000)) { // Check every 30 seconds
    float voltageChange = currentVoltage - lastBatteryVoltage;
    
    if (voltageChange > 0.02) { // Voltage increased significantly
      chargingState = CHARGING;
    } else if (voltageChange < -0.02) { // Voltage decreased significantly
      chargingState = DISCHARGING;
    }
    // If change is small, keep current state (stable)
    
    lastBatteryVoltage = currentVoltage;
    lastBatteryCheck = millis();
  } else if (lastBatteryVoltage == 0) {
    // First reading, just store the value
    lastBatteryVoltage = currentVoltage;
    lastBatteryCheck = millis();
  }
}

float calculateDischargeRateLinearRegression() {
  if (dischargeRateBufferCount < 25) {
    Serial.println("Linear regression: Not enough data");
    return 0.0; // Not enough data for regression
  }
  
  // Extract last 25 readings from discharge rate buffer
  float voltages[25];
  float times[25];
  
  Serial.printf("Linear regression: Processing %d data points\n", dischargeRateBufferCount);
  
  for (int i = 0; i < 25; i++) {
    voltages[i] = dischargeRateBuffer[i].battery_voltage;
    
    // Filter out battery voltage spikes from float reboots (ignore values > 4.5V)
    if (voltages[i] > 4.5) {
      Serial.printf("  Point %d: V=%.3f (IGNORED - spike >4.5V)\n", i, voltages[i]);
      continue; // Skip this point in regression calculation
    }
    
    // Use relative time based on 600s intervals instead of absolute uptime_s
    times[i] = i * 600.0; // 0, 600, 1200, 1800... seconds
    Serial.printf("  Point %d: V=%.3f, rel_time=%.0f\n", i, voltages[i], times[i]);
  }
  
  // Calculate linear regression: y = mx + b
  // where y = voltage, x = time, m = slope (discharge rate)
  
  float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
  int validPoints = 0;
  
  // Count valid points (excluding filtered spikes) and calculate regression
  for (int i = 0; i < 25; i++) {
    if (voltages[i] <= 4.5) { // Only include valid battery readings
      sumX += times[i];
      sumY += voltages[i];
      sumXY += times[i] * voltages[i];
      sumX2 += times[i] * times[i];
      validPoints++;
    }
  }
  
  int n = validPoints;
  if (n < 10) {
    Serial.printf("  Not enough valid points after filtering: %d\n", n);
    return 0.0;
  }
  
  // Calculate slope (m) using least squares formula
  float denominator = n * sumX2 - sumX * sumX;
  if (abs(denominator) < 0.001) {
    return 0.0; // Avoid division by zero
  }
  
  float slope = (n * sumXY - sumX * sumY) / denominator;
  
  // Calculate correlation coefficient (R²) to assess regression quality
  float meanX = sumX / n;
  float meanY = sumY / n;
  
  float ssTotal = 0, ssResidual = 0;
  for (int i = 0; i < n; i++) {
    float predictedY = slope * (times[i] - meanX) + meanY;
    ssTotal += (voltages[i] - meanY) * (voltages[i] - meanY);
    ssResidual += (voltages[i] - predictedY) * (voltages[i] - predictedY);
  }
  
  float rSquared = 1.0 - (ssResidual / ssTotal);
  
  Serial.printf("Linear regression results:\n");
  Serial.printf("  Slope: %.6f V/s\n", slope);
  Serial.printf("  R²: %.3f\n", rSquared);
  Serial.printf("  Discharge rate: %.6f V/s\n", -slope);
  
  // Only trust regression if correlation is reasonable (R² > 0.01)
  if (rSquared < 0.01) {
    Serial.printf("  Poor correlation (R² < 0.01), returning 0.0\n");
    return 0.0; // Poor correlation, data too noisy
  }
  
  // Return negative slope (discharge rate should be positive when voltage is decreasing)
  float dischargeRate = -slope;
  
  // Handle ultra-stable battery: if slope is essentially zero but we have any voltage decrease,
  // apply minimum discharge rate for 30-day operation
  if (dischargeRate < 0.0000001 && rSquared > 0.01) {
    // Check if there's any voltage decrease in the dataset
    float firstVoltage = voltages[0];
    float lastVoltage = voltages[24];
    if (lastVoltage < firstVoltage) {
      dischargeRate = (firstVoltage - lastVoltage) / (24 * 600.0); // Total drop over time period
      Serial.printf("  Applied minimum discharge rate: %.6f V/s\n", dischargeRate);
    }
  }
  
  Serial.printf("  Returning discharge rate: %.6f V/s\n", dischargeRate);
  return dischargeRate;
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
  int cardW = (UI_W - MARGIN * 2)/2;
  
  // 1) Details
  uiCard(MARGIN, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorError);
  tft.setFreeFont(FONT_SIZE_LG);
  tft.setCursor(MARGIN + 15, contentY + cardH / 2 + 15);  // Adjust for FreeFont baseline
  tft.print("Details >");
  
  // 2) Calibration
  contentY += cardH + GAP;
  uiCard(MARGIN, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorError);
  tft.setFreeFont(FONT_SIZE_LG);
  tft.setCursor(MARGIN + 15, contentY + cardH / 2 + 15);  // Adjust for FreeFont baseline
  tft.print("Polygone >");
  
  // 3) Create New Fermentation
  contentY += cardH + GAP;
  uiCard(MARGIN, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorError);
  tft.setFreeFont(FONT_SIZE_LG);
  tft.setCursor(MARGIN + 15, contentY + cardH / 2 + 15);  // Adjust for FreeFont baseline
  tft.print("New Brew >");
  
  // 4) Reboot
  contentY += cardH + GAP;
  uiCard(MARGIN, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorError);
  tft.setFreeFont(FONT_SIZE_LG);
  tft.setCursor(MARGIN + 15, contentY + cardH / 2 + 15);  // Adjust for FreeFont baseline
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
  if (rssiAvailable) {
    tft.printf("RSSI: %ddBm", lastRSSI);
  } else {
    tft.print("RSSI: --");
  }
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
  // Show actual device uptime since boot in seconds
  unsigned long deviceUptime = millis() / 1000;
  tft.printf("Uptime: %lus", deviceUptime);
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
  // Extract just the filename from the path (remove folder name)
  const char* filename = currentFermentationFile;
  const char* lastSlash = strrchr(currentFermentationFile, '/');
  if (lastSlash != NULL) {
    filename = lastSlash + 1;
  }
  uiEllipsize(filename, contentW - 80, ellipsized, sizeof(ellipsized));
  tft.printf("File: %s", ellipsized);
  tft.setCursor(MARGIN + 10, contentY + 64);  // Adjust for FreeFont baseline
  tft.printf("Points: %d (total: %d)", displayDataCount, totalCSVDataLines);
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

void drawPolynomialGraph() {
  tft.fillScreen(uiColorBackground);
  
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar("Polynomial", true, fermentationFileOpen, battPercent);
  
  int contentY = TOPBAR_H + MARGIN;
  int contentW = UI_W - MARGIN * 2;
  
  // Main graph card
  uiCard(POLY_GRAPH_X, POLY_GRAPH_Y, POLY_GRAPH_W, POLY_GRAPH_H, CARD_RADIUS);
  
  // Title
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_MD);
  tft.setCursor(POLY_GRAPH_X + 15, POLY_GRAPH_Y + 14);
  tft.print("Calibration Polynomial");
  
  // Check if calibration is valid
  if (!calibCoeffs.isValid) {
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(POLY_GRAPH_X + 10, POLY_GRAPH_Y + 40);
    tft.print("No valid calibration data");
    return;
  }
  
  // Generate graph points and draw
  calculateGraphPoints();
  drawGraphAxes();
  drawGraphCurve();
  
  uiDrawBottomNav(TAB_MORE);
}

void calculateGraphPoints() {
  // Generate 91 points (0° to 90°) using current calibration polynomial
  for (int i = 0; i <= 90; i++) {
    float sg = calculateGravity(i);
    // Clamp SG to valid range (0.900 to 1.200)
    if (sg < 0.900) sg = 0.900;
    if (sg > 1.200) sg = 1.200;
    
    // Convert to screen coordinates
    int x = POLY_GRAPH_INNER_X + (i * (POLY_GRAPH_INNER_W - 1) / 90);
    int y = POLY_GRAPH_INNER_Y + POLY_GRAPH_INNER_H - 1 - ((sg - 0.900) / (1.200 - 0.900) * (POLY_GRAPH_INNER_H - 20));
    
    // Store point for drawing
    // Note: Using static arrays would be more efficient for production
    graphPoints[i].x = x;
    graphPoints[i].y = y;
    graphPoints[i].valid = true;
  }
}

void drawGraphAxes() {
  // Draw axes
  tft.drawLine(POLY_GRAPH_INNER_X, POLY_GRAPH_INNER_Y + POLY_GRAPH_INNER_H - 1, 
                POLY_GRAPH_INNER_X + POLY_GRAPH_INNER_W - 1, POLY_GRAPH_INNER_Y, POLY_GRAPH_AXIS_COLOR);
  tft.drawLine(POLY_GRAPH_INNER_X, POLY_GRAPH_INNER_Y, 
                POLY_GRAPH_INNER_X + POLY_GRAPH_INNER_W - 1, POLY_GRAPH_INNER_Y + POLY_GRAPH_INNER_H - 1, POLY_GRAPH_AXIS_COLOR);
  
  // Draw Y-axis labels (SG values)
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  
  // SG labels: 0.900 to 1.200
  float sgLabels[] = {0.900, 0.950, 1.000, 1.050, 1.100, 1.150, 1.200};
  for (int i = 0; i < 7; i++) {
    int y = POLY_GRAPH_INNER_Y + POLY_GRAPH_INNER_H - 1 - ((sgLabels[i] - 0.900) / (1.200 - 0.900) * (POLY_GRAPH_INNER_H - 20));
    tft.setCursor(POLY_GRAPH_INNER_X - 25, y);
    tft.printf("%.3f", sgLabels[i]);
  }
  
  // Draw X-axis labels (angles)
  int angleLabels[] = {0, 15, 30, 45, 60, 75, 90};
  for (int i = 0; i < 7; i++) {
    int x = POLY_GRAPH_INNER_X + (angleLabels[i] * (POLY_GRAPH_INNER_W - 1) / 90);
    int y = POLY_GRAPH_INNER_Y + POLY_GRAPH_INNER_H - 1 + 10;
    tft.setCursor(x, y);
    tft.printf("%d°", angleLabels[i]);
  }
  
  // Draw grid lines
  tft.setTextColor(POLY_GRAPH_GRID_COLOR);
  for (int i = 1; i < 6; i++) {
    int x = POLY_GRAPH_INNER_X + (i * (POLY_GRAPH_INNER_W - 1) / 6);
    tft.drawLine(x, POLY_GRAPH_INNER_Y, x, POLY_GRAPH_INNER_Y + POLY_GRAPH_INNER_H - 1, POLY_GRAPH_GRID_COLOR);
  }
  
  for (int i = 1; i < 4; i++) {
    float sg = 0.900 + (i * 0.050);
    int y = POLY_GRAPH_INNER_Y + POLY_GRAPH_INNER_H - 1 - ((sg - 0.900) / (1.200 - 0.900) * (POLY_GRAPH_INNER_H - 20));
    tft.drawLine(POLY_GRAPH_INNER_X, y, POLY_GRAPH_INNER_X + POLY_GRAPH_INNER_W - 1, y, POLY_GRAPH_GRID_COLOR);
  }
}

void drawGraphCurve() {
  // Draw smooth curve through all points
  for (int i = 0; i < 90; i++) {
    if (!graphPoints[i].valid) continue;
    
    if (i > 0 && graphPoints[i-1].valid) {
      tft.drawLine(graphPoints[i-1].x, graphPoints[i-1].y, 
                    graphPoints[i].x, graphPoints[i].y, POLY_GRAPH_CURVE_COLOR);
    }
  }
}

void checkWaitTimeout() {
  if (!waitActive) return;
  
  if (millis() - waitStartTime >= waitDuration) {
    stopWait();
    
    // Handle different wait scenarios
    if (calibMode == CALIB_APPLYING) {
      // Apply timeout - go directly to Live View
      currentMode = LIVE_VIEW;
      calibMode = CALIB_IDLE;
      calibrationModeActive = false;
      screenDirty = true;
      staticElementsDrawn = false; // Force complete redraw to clear artifacts
      Serial.println("Apply timeout - calibration complete, returning to Live View");
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

// Graph cursor helper functions
void formatTimeFromDataPoint(int index, char* buf, int bufSize) {
  if (index < 0 || index >= displayDataCount) {
    snprintf(buf, bufSize, "0m");
    return;
  }
  
  // Calculate time based on data point index with 600s (10 minute) intervals
  // For LOAD_ALL mode, scale the index to represent the actual total CSV data points
  int actualIndex = index;
  if (currentLoadMode == LOAD_ALL && totalCSVDataLines > 0 && displayDataCount > 0) {
    // Scale the cursor index to the total CSV data points
    actualIndex = (int)((float)index / displayDataCount * totalCSVDataLines);
  }
  
  uint32_t totalSeconds = actualIndex * 600;
  
  // Convert to days, hours, minutes
  int days = totalSeconds / 86400;
  int hours = (totalSeconds % 86400) / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  
  if (days > 0) {
    snprintf(buf, bufSize, "%dd %dh %dm", days, hours, minutes);
  } else if (hours > 0) {
    snprintf(buf, bufSize, "%dh %dm", hours, minutes);
  } else {
    snprintf(buf, bufSize, "%dm", minutes);
  }
}

void updateCursorPosition() {
  // Convert cursor position (0.0 to 1.0) to data index
  cursorIndex = (int)(cursorPosition * (displayDataCount - 1));
  if (cursorIndex < 0) cursorIndex = 0;
  if (cursorIndex >= displayDataCount) cursorIndex = displayDataCount - 1;
}

void drawCursorValueDisplay(int x, int y, bool showRight) {
  if (cursorIndex < 0 || cursorIndex >= displayDataCount) return;
  
  payload_t data = displayDataBuffer[(displayDataIndex - displayDataCount + cursorIndex + MAX_DATA_POINTS) % MAX_DATA_POINTS];
  
  char buf[64];
  int lineHeight = 16;
  int textY = y;
  
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setTextDatum(TL_DATUM); // Always use top-left datum for consistent positioning
  
  // SG value (blue)
  tft.setTextColor(currentTheme->graphBlue);
  snprintf(buf, sizeof(buf), "SG: %.4f", data.density);
  int16_t textWidth = tft.textWidth(buf);
  int textX = showRight ? x + 4 : x - textWidth - 4;
  tft.setCursor(textX, textY);
  tft.print(buf);
  textY += lineHeight;
  
  // Temperature value (green)
  tft.setTextColor(currentTheme->graphGreen);
  snprintf(buf, sizeof(buf), "Temp: %.1f°C", data.temperature);
  textWidth = tft.textWidth(buf);
  textX = showRight ? x + 4 : x - textWidth - 4;
  tft.setCursor(textX, textY);
  tft.print(buf);
  textY += lineHeight;
  
  // ABV value (purple) - if available
  if (ogCaptured) {
    tft.setTextColor(currentTheme->graphPurple);
    float abv = calculateABV(originalGravity, data.density);
    snprintf(buf, sizeof(buf), "ABV: %.1f%%", abv);
    textWidth = tft.textWidth(buf);
    textX = showRight ? x + 4 : x - textWidth - 4;
    tft.setCursor(textX, textY);
    tft.print(buf);
    textY += lineHeight;
  }
  
  // Time value (muted)
  tft.setTextColor(uiColorTextMuted);
  char timeBuf[32];
  formatTimeFromDataPoint(cursorIndex, timeBuf, sizeof(timeBuf));
  snprintf(buf, sizeof(buf), "Time: %s", timeBuf);
  textWidth = tft.textWidth(buf);
  textX = showRight ? x + 4 : x - textWidth - 4;
  tft.setCursor(textX, textY);
  tft.print(buf);
}

// Graph button handler functions
void handleGraphButton6h() {
  currentLoadMode = LOAD_6H;
  if (mountSDTemporarily()) {
    if (loadHistoricalDataFromCSV(currentFermentationFile, LOAD_6H, MAX_DATA_POINTS, 1)) {
      cursorPosition = 1.0;
      cursorIndex = displayDataCount - 1;
      screenDirty = true;
      Serial.println("Loaded 6h data");
    }
    dismountSD();
  } else {
    Serial.println("Failed to mount SD for 6h data load");
  }
}

void handleGraphButton24h() {
  currentLoadMode = LOAD_24H;
  if (mountSDTemporarily()) {
    if (loadHistoricalDataFromCSV(currentFermentationFile, LOAD_24H, MAX_DATA_POINTS, 2)) {
      cursorPosition = 1.0;
      cursorIndex = displayDataCount - 1;
      screenDirty = true;
      Serial.println("Loaded 24h data");
    }
    dismountSD();
  } else {
    Serial.println("Failed to mount SD for 24h data load");
  }
}

void handleGraphButtonAll() {
  currentLoadMode = LOAD_ALL;
  if (mountSDTemporarily()) {
    if (loadHistoricalDataFromCSV(currentFermentationFile, LOAD_ALL, MAX_DATA_POINTS, 1)) {
      cursorPosition = 1.0;
      cursorIndex = displayDataCount - 1;
      screenDirty = true;
      Serial.println("Loaded all data");
    }
    dismountSD();
  } else {
    Serial.println("Failed to mount SD for all data load");
  }
}

void handleGraphButtonLeft() {
  cursorPosition -= 0.05;
  if (cursorPosition < 0.0) cursorPosition = 0.0;
  updateCursorPosition();
  screenDirty = true;
  Serial.printf("Cursor moved left to position %.2f (index %d)\n", cursorPosition, cursorIndex);
}

void handleGraphButtonRight() {
  cursorPosition += 0.05;
  if (cursorPosition > 1.0) cursorPosition = 1.0;
  updateCursorPosition();
  screenDirty = true;
  Serial.printf("Cursor moved right to position %.2f (index %d)\n", cursorPosition, cursorIndex);
}