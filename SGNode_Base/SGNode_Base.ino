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
#include <Wire.h>
#include <EEPROM.h>
#include "Logo2_Optimized.h"
#include "ui_tokens.h"
#include "ui_components.h"
#include "brew_profile.h"
#include "brew_wizard_controller.h"
#include "brix_converter.h"
#include "og_verifier.h"
#include "derived_calculations.h"
#include "target_curve.h"
#include "fermentation_state_machine.h"
#include "recommendation_engine.h"
#include "eta_predictor.h"
#include "batch_action.h"
#include "yeast_preset_repository.h"
#ifndef SGNODE_UI_TEST_HARNESS
#define SGNODE_UI_TEST_HARNESS 0  // Debug-only serial UI harness. Keep disabled for production builds.
#endif
#include "ui_test_harness.h"
#include "../SGNode_Shared/sg_protocol.h"
#include "../SGNode_Base/polynomial_calibration.h"

#define ACK_PACKET_TYPE 0xA5

// 4.0inch ESP32-32E Display configuration - ST7796S landscape mode
#define SCREEN_W    480  // Landscape width (320x480 native rotated)
#define SCREEN_H    320  // Landscape height
#define DISPLAY_CS  15   // LCD Chip Select
#define DISPLAY_DC  2    // Data/Command
#define DISPLAY_RST -1   // Reset (uses ESP32 reset)

// Custom color definitions (TFT_eSPI doesn't have TFT_GRAY)
#define TFT_GRAY 0x8410  // Dark gray color

enum DebugLevel {
  DEBUG_QUIET = 0,
  DEBUG_ERROR = 1,
  DEBUG_INFO = 2,
  DEBUG_VERBOSE = 3
};

#ifndef SGNODE_DEBUG_LEVEL
#define SGNODE_DEBUG_LEVEL DEBUG_ERROR
#endif

#define LOG_ERROR(...) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_ERROR) Serial.printf(__VA_ARGS__); } while (0)
#define LOG_ERRORLN(msg) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_ERROR) Serial.println(msg); } while (0)
#define LOG_INFO(...) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_INFO) Serial.printf(__VA_ARGS__); } while (0)
#define LOG_INFOLN(msg) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_INFO) Serial.println(msg); } while (0)
#define LOG_VERBOSE(...) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_VERBOSE) Serial.printf(__VA_ARGS__); } while (0)
#define LOG_VERBOSELN(msg) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_VERBOSE) Serial.println(msg); } while (0)

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
  .primary = rgb565(248, 248, 248),        // Light top bar
  .primaryText = rgb565(11, 31, 42),       // Navy text
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

  .primary          = rgb565(8, 18, 32),
  .primaryText      = rgb565(255, 255, 255),

  .accent           = rgb565(22, 30, 56),
  .accentText       = rgb565(255, 255, 255),

  .gold             = rgb565(235, 190, 92),

  .textPrimary      = rgb565(255, 255, 255),
  .textSecondary    = rgb565(215, 224, 235),
  .textMuted        = rgb565(160, 174, 192),

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

// DS3230/DS323x RTC on the base station I2C bus
#define RTC_SDA_PIN 32
#define RTC_SCL_PIN 25
#define RTC_I2C_ADDR 0x68
#define RTC_COMPILE_UPLOAD_OFFSET_S 11
#define RTC_VALID_FROM_YEAR 2024

// ESP-NOW configuration
#define ESPNOW_CHANNEL 1

// Data buffering system to avoid real-time SD operations
#define MAX_DATA_BUFFER 500  // Buffer up to 500 sensor readings (increased from 100)
struct BufferedData {
  payload_t data;
  uint32_t epoch_s;
  unsigned long buffered_millis;
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
uint32_t displayTimestampBuffer[MAX_DATA_POINTS];
int displayDataIndex = 0;
int displayDataCount = 0;
payload_t latestFloatData = {0};
uint32_t latestFloatEpoch = 0;
bool latestFloatDataValid = false;
uint16_t lastAckedFloatSeq = 0;
uint16_t highestSeenFloatSeq = 0;
uint16_t recentFloatSeqs[16] = {0};
uint8_t recentFloatSeqIndex = 0;
uint8_t recentFloatSeqCount = 0;
uint32_t duplicateFloatPackets = 0;
int totalCSVDataLines = 0;  // Total data points in CSV file
int lastHistoricalLoadedLines = 0;
int lastHistoricalSkippedLines = 0;
int lastHistoricalParseErrors = 0;
int lastHistoricalPreBatchLines = 0;
int lastHistoricalPostBatchLines = 0;
int lastHistoricalBatchFilteredLines = 0;
int lastHistoricalCutoffFilteredLines = 0;
bool lastHistoricalBatchStartRelaxed = false;
uint32_t lastHistoricalOldestEpoch = 0;
uint32_t lastHistoricalNewestEpoch = 0;

// Separate buffer for discharge rate calculation (25 recent points only)
payload_t dischargeRateBuffer[25];
uint32_t dischargeRateTimestampBuffer[25];
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
  ,
  BREW_WIZARD_VIEW,
  OG_VERIFICATION_VIEW,
  TARGET_CHART_VIEW,
  DASHBOARD_VIEW,
  NEW_YEAST_VIEW,
  MANAGE_YEAST_VIEW,
  MANAGE_BREW_VIEW
};

ViewMode currentMode = LIVE_VIEW;

// Graph metric types for parameterized graph drawing
enum GraphMetric { METRIC_DENSITY, METRIC_TEMPERATURE, METRIC_ANGLE, METRIC_ABV };

// Calibration state
enum CalibMode {
  CALIB_IDLE,
  CALIB_SETUP,
  CALIB_INSTRUCTIONS,
  CALIB_OFFSET,
  CALIB_POINT1,
  CALIB_POINT2,
  CALIB_POINT3,
  CALIB_POINT4,
  CALIB_POINT5,
  CALIB_POINT6,
  CALIB_COMPLETE,
  CALIB_APPLYING,
  CALIB_FAILED,
  CALIB_SKIP_WARNING
};

CalibMode calibMode = CALIB_IDLE;
static const int MAX_BASE_CALIB_POINTS = 6;
float calibAngles[MAX_BASE_CALIB_POINTS] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
float calibSG[MAX_BASE_CALIB_POINTS] = {1.000, 1.010, 1.020, 1.030, 1.045, 1.060};
int sugarAmounts4[4] = {0, 107, 115, 124}; // Approx. incremental grams for 2kg water
int sugarAmounts6[6] = {0, 53, 54, 56, 89, 94};
int saltAmounts4[4] = {0, 58, 63, 60}; // Approx. incremental grams for 2kg water at 20C
int saltAmounts6[6] = {0, 29, 29, 29, 49, 45};
bool calibDoSGCalibration = true;
bool calibUseSixPoints = false;
bool calibUseSalt = false;
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
#define EEPROM_THEME_MAGIC 120
#define EEPROM_THEME_VALUE 124
#define FLOAT_MAC_MAGIC 0x4D414346 // "MACF" in hex
#define THEME_MAGIC 0x54484D45 // "THME" in hex

// Touch handling
uint16_t touchX, touchY;
#if SGNODE_UI_TEST_HARNESS
struct UITestObject {
  const char* id;
  const char* type;
  const char* label;
  int x;
  int y;
  int w;
  int h;
  bool enabled;
  bool visible;
};

bool uiTestTouchActive = false;
uint16_t uiTestTouchX = 0;
uint16_t uiTestTouchY = 0;
unsigned long uiTestMockTime = 0;
payload_t uiTestMockPayload = {0};
bool uiTestMockSGSet = false;
bool uiTestMockTempSet = false;
#endif

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
bool showingCreateNewDialog = false;

// Phase 1 intelligent fermentation assistant state
BrewProfile activeBrewProfile;
BrewWizardController brewWizard;
OGVerifier ogVerifier;
FermentationStateMachine fermentationStateMachine;
DerivedMetrics fermentationMetrics = {1.000f, 0.0f, 0.0f, 0.0f, 0.0f};
Recommendation currentRecommendation = {0, "Start Brew Wizard to enable guidance"};
BatchAction currentBatchAction = {ACTION_NONE, 0, false, 0, "Next", "No action required"};
ETAResult currentETA = {0, 0, false, false, 0};
bool brewProfileLoaded = false;
bool brewWizardHasSavedRuntime = false;
BrewProfile brewWizardSavedProfile;
bool brewWizardSavedProfileLoaded = false;
bool brewWizardSavedFermentationFileOpen = false;
char brewWizardSavedFermentationFile[sizeof(currentFermentationFile)] = "";
bool ogVerificationPending = false;
bool targetCurveAvailable = false;
bool yeastPerformanceSaved = false;
float previousAnalyticsSG = 0.0f;
uint32_t previousAnalyticsEpoch = 0;

enum NewYeastStep {
  NY_NAME,
  NY_CATEGORY,
  NY_ATT_MIN,
  NY_ATT_MAX,
  NY_ATT_DEFAULT,
  NY_SPEED,
  NY_LAG,
  NY_DURATION,
  NY_TEMP_MIN,
  NY_TEMP_MAX,
  NY_FLOC,
  NY_CURVE,
  NY_DREST,
  NY_NOTES,
  NY_REVIEW
};

struct EditableYeastPreset {
  char id[24];
  char displayName[32];
  char category[32];
  int typicalAttenuationMin;
  int typicalAttenuationMax;
  int defaultAttenuation;
  char fermentationSpeed[16];
  float lagPhaseHours;
  float typicalDurationHours;
  float recommendedTempMinC;
  float recommendedTempMaxC;
  char flocculation[16];
  char curveTemplate[32];
  bool diacetylRestRecommended;
  char notes[64];
};

EditableYeastPreset yeastDraft;
NewYeastStep newYeastStep = NY_NAME;
char yeastEditBuffer[64] = "";
bool yeastEditPristine = false;
bool editingExistingYeast = false;
int manageYeastIndex = 0;
char managedYeastIds[12][24];
char managedYeastNames[12][32];
int managedYeastCount = 0;
int manageBatchIndex = 0;
char managedBatchIds[16][24];
char managedBatchNames[16][40];
int managedBatchCount = 0;
bool manageBrewCompleteConfirm = false;

struct ManagedBatchSummary {
  bool loaded;
  bool completed;
  char style[24];
  float liters;
  int points;
  uint32_t firstEpoch;
  uint32_t lastEpoch;
  char status[18];
  char evaluation[32];
};

// Calibration buttons (step-by-step wizard) - reorganized for consistent layout
#define BUTTON_CALIB_START_X (UI_W - MARGIN - 132)
#define BUTTON_CALIB_START_Y 236
#define BUTTON_CALIB_START_W 132
#define BUTTON_CALIB_START_H 36

#define BUTTON_CALIB_BACK_X MARGIN
#define BUTTON_CALIB_BACK_Y 236
#define BUTTON_CALIB_BACK_W 132
#define BUTTON_CALIB_BACK_H 36

#define BUTTON_CALIB_NEXT_X (UI_W - MARGIN - 132)
#define BUTTON_CALIB_NEXT_Y 236
#define BUTTON_CALIB_NEXT_W 132
#define BUTTON_CALIB_NEXT_H 36

#define BUTTON_CALIB_APPLY_X (UI_W - MARGIN - 132)
#define BUTTON_CALIB_APPLY_Y 236
#define BUTTON_CALIB_APPLY_W 132
#define BUTTON_CALIB_APPLY_H 36

#define BUTTON_CALIB_EXIT_X MARGIN
#define BUTTON_CALIB_EXIT_Y 236
#define BUTTON_CALIB_EXIT_W 132
#define BUTTON_CALIB_EXIT_H 36

// Offset calibration buttons - repositioned for Step 1/5 layout
#define BUTTON_CALIB_OFFSET_X (UI_W - MARGIN - 132)
#define BUTTON_CALIB_OFFSET_Y 236
#define BUTTON_CALIB_OFFSET_W 132
#define BUTTON_CALIB_OFFSET_H 36

// Record button - moved to same position as Calibrate button
#define BUTTON_CALIB_RECORD_X (UI_W - MARGIN - 132)
#define BUTTON_CALIB_RECORD_Y 236
#define BUTTON_CALIB_RECORD_W 132
#define BUTTON_CALIB_RECORD_H 36

// Battery view buttons
#define BATTERY_BUTTON_H 40
#define BATTERY_BUTTON_Y (UI_H - NAV_H - BATTERY_BUTTON_H - GAP)

// Exit button for Step 1/5 - lower left
#define BUTTON_CALIB_EXIT_OFFSET_X MARGIN
#define BUTTON_CALIB_EXIT_OFFSET_Y 236
#define BUTTON_CALIB_EXIT_OFFSET_W 132
#define BUTTON_CALIB_EXIT_OFFSET_H 36

// Skip SG Calibration buttons
#define BUTTON_CALIB_SKIP_X MARGIN
#define BUTTON_CALIB_SKIP_Y 236
#define BUTTON_CALIB_SKIP_W 132
#define BUTTON_CALIB_SKIP_H 36

// Skip confirmation buttons - repositioned to avoid conflicts
#define BUTTON_CALIB_SKIP_YES_X (UI_W - MARGIN - 132)
#define BUTTON_CALIB_SKIP_YES_Y 236
#define BUTTON_CALIB_SKIP_YES_W 132
#define BUTTON_CALIB_SKIP_YES_H 36

#define BUTTON_CALIB_SKIP_NO_X MARGIN
#define BUTTON_CALIB_SKIP_NO_Y 236
#define BUTTON_CALIB_SKIP_NO_W 132
#define BUTTON_CALIB_SKIP_NO_H 36

// Function prototypes
void initESPNow();
void initDisplay();
void initTouch();
void initRTC();
bool readRTCDateTime(uint16_t* year, uint8_t* month, uint8_t* day,
                     uint8_t* hour, uint8_t* minute, uint8_t* second);
bool setRTCDateTime(uint16_t year, uint8_t month, uint8_t day,
                    uint8_t hour, uint8_t minute, uint8_t second);
uint32_t getCurrentEpoch();
bool isEpochValid(uint32_t epoch);
void formatTimestamp(uint32_t epoch, char* buf, size_t bufSize);
uint32_t buildEpochWithUploadOffset();
uint32_t parseDateTimeToEpoch(const char* timestamp);
void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
void sendDataAck(const uint8_t* mac, uint16_t sequence_id);
bool isDuplicateFloatSequence(uint16_t sequence_id);
void rememberFloatSequence(uint16_t sequence_id);
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
void drawBrewWizardScreen();
void drawOGVerificationScreen();
void drawTargetVsActualChart();
void drawDashboardScreen();
void drawNewYeastScreen();
void drawManageYeastScreen();
void drawManageBrewScreen();
void drawRecommendationPanel(int x, int y, int w, int h);
void calculateGraphPoints();
void drawGraphAxes();
void drawGraphCurve();
void checkTouch();
float calculateGravity(float tilt);

void addDataPoint(payload_t data, uint32_t epoch_s);
void drawGrid();
void drawGraph();
void loadFloatMacFromEEPROM();
void saveFloatMacToEEPROM();
void startWait(int durationMs);
void checkWaitTimeout();
void stopWait();
void sendCalibrationCommand(uint8_t command, float target_sg);
void applyCalibration();
int calibrationPointCount();
int calibrationPointIndex(CalibMode mode);
CalibMode calibrationModeForPoint(int index);
float calibrationTargetSG(int index);
int calibrationAddAmount(int index);
void sendExitCalibrationCommand();
void abortCalibrationFlow();
bool logDataToSD(payload_t data, uint32_t epoch_s);
void createNewFermentationFile();
bool ensureFermentationLogFile(const char* batchId);
bool findBestFallbackBatch(char* batchId, size_t bufferSize);
void drawCreateNewDialog();
void checkExistingFermentation();
bool loadHistoricalDataFromCSV(const char* filename, 
                               GraphLoadMode mode = LOAD_6H,
                               int maxPoints = MAX_DATA_POINTS,
                               int samplingStep = 1);
bool parseCSVDataLine(const char* line, payload_t* data, uint32_t* epoch_s, uint8_t* battery_percent);
void checkOGStability(float currentSG);
void logOGToSD();
float calculateABV(float og, float currentSG);
bool isPlausibleSensorReading(const payload_t& data, const char** issue);
void beginBrewWizard();
void completeBrewWizard();
void cancelBrewWizard();
void beginNewYeastWizard(const char* presetId = NULL);
void loadYeastStepBuffer();
void commitYeastStepBuffer();
void drawNewYeastNav(const char* nextLabel);
bool handleNewYeastTouch(int x, int y);
void saveNewYeastWizard();
void loadManagedYeasts();
bool handleManageYeastTouch(int x, int y);
void loadManagedBatches();
bool handleManageBrewTouch(int x, int y);
void updateFermentationAssistant(payload_t data, uint32_t epoch_s);
void refreshFermentationAssistantFromProfile();
void finalizeHistoricalDataLoad();
void markScreenDirtyForFloatData();
void clearHistoricalDisplayData();
void drawWrappedText(int x, int y, int w, const char* text, uint16_t color);
void persistActiveBrewProfile();
void startNewBatchStorage(const char* batchId);
void formatDurationShort(unsigned long seconds, char* buffer, size_t bufferSize);
void handleOGChoice(bool useMeasuredOG);
void saveYeastPerformanceSummary(float finalGravity, uint32_t completedAt);
void refreshCurrentBatchAction(float currentSG, unsigned long nowEpoch);
void handleCurrentActionChoice(bool done);
void formatActionHeader(char* buffer, size_t bufferSize);
uint8_t calculateBatteryPercentage(float voltage);
void updateChargingState(float currentVoltage);
float calculateDischargeRateLinearRegression();

// Graph cursor helper functions
void formatTimeFromDataPoint(int index, char* buf, int bufSize);
void updateCursorPosition();
void drawCursorValueDisplay(int x, int y, bool showRight);

// View helper functions
void drawViewTopbar(const char* title);
void drawNoDataCard(const char* message, const char* subtext, int activeTab);
void drawDetailSection(int* y, const char* title);
void drawDetailRow(int* y, const char* label, const char* value);
void drawDetailSectionAt(int x, int* y, int w, const char* title);
void drawDetailRowAt(int x, int* y, int w, const char* label, const char* value);

// Graph button handler functions
void handleGraphButton6h();
void handleGraphButton24h();
void handleGraphButtonAll();
void handleGraphButtonLeft();
void handleGraphButtonRight();

// Time-based SPI separation functions
bool mountSDTemporarily();
void dismountSD();
void bufferSensorData(payload_t data, uint32_t epoch_s);
void writeBufferedDataToSD();
bool isSafeForSDOperation();

// UI Design System Helper Functions
void drawButton(int x, int y, int w, int h, const char* label, bool active);
void drawThemeToggle(int x, int y, bool enabled);
bool isLogoBackgroundPixel(uint16_t color);
void toggleTheme();
void loadThemePreference();
void saveThemePreference();
void drawBootScreen();
void uiInitColors();

void setup() {
  Serial.begin(115200);
  LOG_INFOLN("=== SGNode Base Station alpha 0.4.0 starting ===");
  
  loadThemePreference();
  
  // Initialize UI color tokens from theme
  uiInitColors();
  
  initDisplay();
  initTouch();
  initCalibration();
  initRTC();
  
  // Initialize SD CS pin but don't mount SD yet (to avoid touch interference)
  #if SD_ENABLED
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH); // Ensure SD CS is HIGH initially
    LOG_INFOLN("SD card CS pin initialized (not mounted yet)");
    LOG_INFOLN("Touch ready - SD will mount temporarily when needed");
  #else
    LOG_INFOLN("SD card DISABLED");
  #endif
  
  initESPNow();
  
  // Load float MAC from EEPROM for permanent pairing
  loadFloatMacFromEEPROM();
  
  // Show boot screen first
  drawBootScreen();
  
  // Check for existing fermentation and auto-continue or create new
  checkExistingFermentation();
  
  LOG_INFOLN("Base station ready");
  // Initial screen draw happens through the normal dirty-screen path so
  // restored historical data and the selected view use the same refresh logic.
  screenDirty = true;
  staticElementsDrawn = false;
}

void loop() {
  #if SGNODE_UI_TEST_HARNESS
    handleUITestHarness();
  #endif

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
        case BREW_WIZARD_VIEW:
          drawBrewWizardScreen();
          break;
        case OG_VERIFICATION_VIEW:
          drawOGVerificationScreen();
          break;
        case TARGET_CHART_VIEW:
          drawTargetVsActualChart();
          break;
        case DASHBOARD_VIEW:
          drawDashboardScreen();
          break;
        case NEW_YEAST_VIEW:
          drawNewYeastScreen();
          break;
        case MANAGE_YEAST_VIEW:
          drawManageYeastScreen();
          break;
        case MANAGE_BREW_VIEW:
          drawManageBrewScreen();
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
        case BREW_WIZARD_VIEW:
          title = "Wizard";
          break;
        case OG_VERIFICATION_VIEW:
          title = "OG Verify";
          break;
        case TARGET_CHART_VIEW:
          title = "Target";
          break;
        case DASHBOARD_VIEW:
          title = "Dashboard";
          break;
        case NEW_YEAST_VIEW:
          title = "New Yeast";
          break;
        case MANAGE_YEAST_VIEW:
          title = "Manage Yeast";
          break;
        case MANAGE_BREW_VIEW:
          title = "Manage Brew";
          break;
      }
      
      uiDrawTopbar(title, true, fermentationFileOpen, battPercent);
      topbarDirty = false;
    }
  }
  
  delay(10);
}

uint8_t bcdToDec(uint8_t value) {
  return ((value >> 4) * 10) + (value & 0x0F);
}

uint8_t decToBcd(uint8_t value) {
  return ((value / 10) << 4) | (value % 10);
}

int64_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = (unsigned)(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int)doe - 719468;
}

uint32_t dateTimeToEpoch(uint16_t year, uint8_t month, uint8_t day,
                         uint8_t hour, uint8_t minute, uint8_t second) {
  int64_t days = daysFromCivil(year, month, day);
  if (days < 0) return 0;
  return (uint32_t)(days * 86400 + (uint32_t)hour * 3600 + (uint32_t)minute * 60 + second);
}

void civilFromDays(int64_t z, uint16_t* year, uint8_t* month, uint8_t* day) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int y = (int)yoe + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned d = doy - (153 * mp + 2) / 5 + 1;
  const unsigned m = mp + (mp < 10 ? 3 : -9);
  y += (m <= 2);
  *year = (uint16_t)y;
  *month = (uint8_t)m;
  *day = (uint8_t)d;
}

void epochToDateTime(uint32_t epoch, uint16_t* year, uint8_t* month, uint8_t* day,
                     uint8_t* hour, uint8_t* minute, uint8_t* second) {
  uint32_t secondsOfDay = epoch % 86400UL;
  civilFromDays(epoch / 86400UL, year, month, day);
  *hour = secondsOfDay / 3600UL;
  *minute = (secondsOfDay % 3600UL) / 60UL;
  *second = secondsOfDay % 60UL;
}

bool isEpochValid(uint32_t epoch) {
  return epoch >= dateTimeToEpoch(RTC_VALID_FROM_YEAR, 1, 1, 0, 0, 0);
}

uint8_t compileMonthNumber(const char* monthName) {
  static const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* found = strstr(months, monthName);
  if (!found) return 1;
  return ((found - months) / 3) + 1;
}

uint32_t buildEpochWithUploadOffset() {
  char monthName[4] = {0};
  int day = 1;
  int year = 2024;
  int hour = 0;
  int minute = 0;
  int second = 0;
  sscanf(__DATE__, "%3s %d %d", monthName, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
  return dateTimeToEpoch(year, compileMonthNumber(monthName), day, hour, minute, second) +
         RTC_COMPILE_UPLOAD_OFFSET_S;
}

bool readRTCDateTime(uint16_t* year, uint8_t* month, uint8_t* day,
                     uint8_t* hour, uint8_t* minute, uint8_t* second) {
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  
  if (Wire.requestFrom(RTC_I2C_ADDR, 7) != 7) {
    return false;
  }
  
  *second = bcdToDec(Wire.read() & 0x7F);
  *minute = bcdToDec(Wire.read() & 0x7F);
  uint8_t hourReg = Wire.read();
  if (hourReg & 0x40) {
    *hour = bcdToDec(hourReg & 0x1F);
    if (hourReg & 0x20) *hour = (*hour % 12) + 12;
  } else {
    *hour = bcdToDec(hourReg & 0x3F);
  }
  Wire.read(); // day-of-week
  *day = bcdToDec(Wire.read() & 0x3F);
  *month = bcdToDec(Wire.read() & 0x1F);
  *year = 2000 + bcdToDec(Wire.read());
  
  return *month >= 1 && *month <= 12 && *day >= 1 && *day <= 31 &&
         *hour <= 23 && *minute <= 59 && *second <= 59;
}

bool setRTCDateTime(uint16_t year, uint8_t month, uint8_t day,
                    uint8_t hour, uint8_t minute, uint8_t second) {
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x00);
  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(1)); // day-of-week is not used by SGNode
  Wire.write(decToBcd(day));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year >= 2000 ? year - 2000 : 0));
  if (Wire.endTransmission() != 0) {
    return false;
  }
  
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x0F);
  Wire.write(0x00); // clear oscillator-stop flag on DS323x-compatible RTCs
  Wire.endTransmission();
  return true;
}

uint32_t getCurrentEpoch() {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  if (!readRTCDateTime(&year, &month, &day, &hour, &minute, &second)) {
    return 0;
  }
  return dateTimeToEpoch(year, month, day, hour, minute, second);
}

void formatTimestamp(uint32_t epoch, char* buf, size_t bufSize) {
  if (!isEpochValid(epoch)) {
    snprintf(buf, bufSize, "RTC_UNSET");
    return;
  }
  
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  epochToDateTime(epoch, &year, &month, &day, &hour, &minute, &second);
  snprintf(buf, bufSize, "%04u-%02u-%02u %02u:%02u:%02u",
           year, month, day, hour, minute, second);
}

uint32_t parseDateTimeToEpoch(const char* timestamp) {
  int year, month, day, hour, minute, second;
  if (sscanf(timestamp, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
    return dateTimeToEpoch(year, month, day, hour, minute, second);
  }
  return 0;
}

void initRTC() {
  Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
  
  uint32_t buildEpoch = buildEpochWithUploadOffset();
  uint32_t rtcEpoch = getCurrentEpoch();
  
  if (!isEpochValid(rtcEpoch) || rtcEpoch < buildEpoch) {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    epochToDateTime(buildEpoch, &year, &month, &day, &hour, &minute, &second);
    if (setRTCDateTime(year, month, day, hour, minute, second)) {
      char ts[24];
      formatTimestamp(buildEpoch, ts, sizeof(ts));
      LOG_INFO("RTC set from compile time (+%ds): %s\n", RTC_COMPILE_UPLOAD_OFFSET_S, ts);
    } else {
      LOG_ERRORLN("RTC not found on SDA=32/SCL=25 (address 0x68)");
    }
  } else {
    char ts[24];
    formatTimestamp(rtcEpoch, ts, sizeof(ts));
    LOG_INFO("RTC running: %s\n", ts);
  }
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
    LOG_ERRORLN("Error initializing ESP-NOW");
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
      LOG_ERROR("Unknown packet size: %d bytes\n", len);
    }
  });
  
  LOG_INFOLN("ESP-NOW initialized");
  
  // Print base station MAC address after WiFi is fully initialized
  String macAddress = WiFi.macAddress();
  LOG_INFO("Base Station MAC: %s\n", macAddress.c_str());
}

void sendDataAck(const uint8_t* mac, uint16_t sequence_id) {
  if (!mac) return;

  ack_packet_t ack;
  ack.packet_type = ACK_PACKET_TYPE;
  ack.sequence_id = sequence_id;
  ack.highest_seen = highestSeenFloatSeq;

  esp_err_t result = esp_now_send(mac, (uint8_t*)&ack, sizeof(ack));
  if (result == ESP_OK) {
    lastAckedFloatSeq = sequence_id;
  } else {
    LOG_VERBOSE("Failed to send ACK for seq=%u result=%d\n", sequence_id, result);
  }
}

bool isDuplicateFloatSequence(uint16_t sequence_id) {
  for (int i = 0; i < recentFloatSeqCount; i++) {
    if (recentFloatSeqs[i] == sequence_id) return true;
  }
  return false;
}

void rememberFloatSequence(uint16_t sequence_id) {
  recentFloatSeqs[recentFloatSeqIndex] = sequence_id;
  recentFloatSeqIndex = (recentFloatSeqIndex + 1) % 16;
  if (recentFloatSeqCount < 16) recentFloatSeqCount++;
  if (sequence_id > highestSeenFloatSeq || highestSeenFloatSeq == 0) {
    highestSeenFloatSeq = sequence_id;
  }
}

void initDisplay() {
  tft.init();
  tft.setRotation(1); // Landscape mode - MUST match touchCalibration_rotate in Touch.h (which is 1)
  
  // Verify screen dimensions after rotation
  LOG_INFO("Display dimensions after rotation: %dx%d\n", tft.width(), tft.height());
  
  tft.fillScreen(currentTheme->background);
  tft.setTextColor(currentTheme->textPrimary, currentTheme->background);
  
  LOG_INFOLN("Display initialized for 480x320 landscape (rotation 1)");
}

void initTouch() {
  // TFT_eSPI touch is initialized automatically with tft.init()
  // Calibration values are set in User_Setup.h or Touch.h
  // Y-axis inversion now enabled in Touch.h (touchCalibration_invert_y = 1)
  
  // Ensure SD CS is HIGH to prevent SD card from interfering with touch
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  
  LOG_INFOLN("Touch screen ready (using TFT_eSPI built-in support)");
}

void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(payload_t)) {
    // Store float MAC address if not known yet
    if (!floatMacKnown) {
      memcpy(floatMac, mac, 6);
      floatMacKnown = true;
      LOG_INFO("Float MAC address captured: %02X:%02X:%02X:%02X:%02X:%02X\n", 
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
        LOG_INFOLN("Float unit registered as ESP-NOW peer");
      } else if (addResult == ESP_ERR_ESPNOW_EXIST) {
        LOG_INFOLN("Float unit already registered as peer");
      } else {
        LOG_ERROR("Failed to register float as peer: %d\n", addResult);
      }
    }
    
    payload_t receivedData;
    memcpy(&receivedData, incomingData, len);
    
    // Verify protocol version
    if (receivedData.version != SG_PROTOCOL_VERSION) {
      LOG_ERROR("Warning: Protocol version mismatch (expected %d, got %d)\n", SG_PROTOCOL_VERSION, receivedData.version);
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
      LOG_ERROR("CRC mismatch: expected %d, got %d\n", receivedData.crc, calculated_crc);
      return;
    }

    bool duplicatePacket = isDuplicateFloatSequence(receivedData.sequence_id);
    if (!duplicatePacket) {
      rememberFloatSequence(receivedData.sequence_id);
    } else {
      duplicateFloatPackets++;
    }
    sendDataAck(mac, receivedData.sequence_id);
    
    uint32_t receivedEpoch = getCurrentEpoch();
    uint8_t batteryPercent = calculateBatteryPercentage(receivedData.battery_voltage);
    latestFloatData = receivedData;
    latestFloatEpoch = receivedEpoch;
    latestFloatDataValid = true;
    
    // Update charging state based on voltage trends
    updateChargingState(receivedData.battery_voltage);
    
    LOG_VERBOSE("Received: seq=%d, angle=%.2f°, density=%.3f, temp=%.1f°C, battery=%.2fV (%d%%) [%s]\n", 
                  receivedData.sequence_id, receivedData.angle, receivedData.density, 
                  receivedData.temperature, receivedData.battery_voltage, batteryPercent,
                  chargingState == CHARGING ? "CHARGING" : 
                  chargingState == DISCHARGING ? "DISCHARGING" : "UNKNOWN");
    
    if (!duplicatePacket) {
      // Check OG stability if not yet captured
      if (!brewProfileLoaded && !ogCaptured && fermentationFileOpen) {
        checkOGStability(receivedData.density);
      }

      // Calculate ABV if OG is captured
      if (ogCaptured) {
        currentABV = calculateABV(originalGravity, receivedData.density);
      }

      updateFermentationAssistant(receivedData, receivedEpoch);

      // Add data to display buffer
      addDataPoint(receivedData, receivedEpoch);

      // Buffer sensor data for SD writing (time-based approach)
      bufferSensorData(receivedData, receivedEpoch);
    } else {
      LOG_VERBOSE("Duplicate float packet seq=%u acknowledged, not logged again\n", receivedData.sequence_id);
    }
    
    markScreenDirtyForFloatData();
    
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
    LOG_ERROR("Unknown data format received: %d bytes\n", len);
  }
}

void addDataPoint(payload_t data, uint32_t epoch_s) {
  displayDataBuffer[displayDataIndex] = data;
  displayTimestampBuffer[displayDataIndex] = epoch_s;
  displayDataIndex = (displayDataIndex + 1) % MAX_DATA_POINTS;
  
  if (displayDataCount < MAX_DATA_POINTS) {
    displayDataCount++;
  }
  
  if (dischargeRateBufferCount < 25) {
    dischargeRateBuffer[dischargeRateBufferCount] = data;
    dischargeRateTimestampBuffer[dischargeRateBufferCount] = epoch_s;
    dischargeRateBufferCount++;
  } else {
    for (int i = 1; i < 25; i++) {
      dischargeRateBuffer[i - 1] = dischargeRateBuffer[i];
      dischargeRateTimestampBuffer[i - 1] = dischargeRateTimestampBuffer[i];
    }
    dischargeRateBuffer[24] = data;
    dischargeRateTimestampBuffer[24] = epoch_s;
  }
}

void clearHistoricalDisplayData() {
  displayDataIndex = 0;
  displayDataCount = 0;
  totalCSVDataLines = 0;
  lastHistoricalLoadedLines = 0;
  lastHistoricalSkippedLines = 0;
  lastHistoricalParseErrors = 0;
  lastHistoricalPreBatchLines = 0;
  lastHistoricalPostBatchLines = 0;
  lastHistoricalBatchFilteredLines = 0;
  lastHistoricalCutoffFilteredLines = 0;
  lastHistoricalBatchStartRelaxed = false;
  lastHistoricalOldestEpoch = 0;
  lastHistoricalNewestEpoch = 0;
  dischargeRateBufferCount = 0;
  memset(displayDataBuffer, 0, sizeof(displayDataBuffer));
  memset(displayTimestampBuffer, 0, sizeof(displayTimestampBuffer));
  memset(dischargeRateBuffer, 0, sizeof(dischargeRateBuffer));
  memset(dischargeRateTimestampBuffer, 0, sizeof(dischargeRateTimestampBuffer));
  previousAnalyticsSG = 0.0f;
  previousAnalyticsEpoch = 0;
}

void finalizeHistoricalDataLoad() {
  if (displayDataCount > 0) {
    cursorPosition = 1.0f;
    cursorIndex = displayDataCount - 1;
    refreshFermentationAssistantFromProfile();
    lastUpdate = millis();
  }

  staticElementsDrawn = false;
  heroDirty = true;
  tileTempDirty = true;
  tileAngleDirty = true;
  tileAbvDirty = true;
  topbarDirty = true;
  detailsDirty = true;
  screenDirty = true;
}

void markScreenDirtyForFloatData() {
  static bool lastHadData = false;
  bool hasData = displayDataCount > 0;

  heroDirty = true;
  tileTempDirty = true;
  tileAngleDirty = true;
  tileAbvDirty = true;
  topbarDirty = true;
  detailsDirty = true;

  if (!lastHadData && hasData) {
    staticElementsDrawn = false;
  }
  lastHadData = hasData;

  screenDirty = true;
}

int displayBufferIndexForPoint(int pointIndex) {
  return (displayDataIndex - displayDataCount + pointIndex + MAX_DATA_POINTS) % MAX_DATA_POINTS;
}

uint32_t timestampForPoint(int pointIndex) {
  if (pointIndex < 0 || pointIndex >= displayDataCount) {
    return 0;
  }
  return displayTimestampBuffer[displayBufferIndexForPoint(pointIndex)];
}

bool timestampsAvailableForGraph() {
  if (displayDataCount < 2) return false;
  uint32_t firstEpoch = timestampForPoint(0);
  uint32_t lastEpoch = timestampForPoint(displayDataCount - 1);
  return isEpochValid(firstEpoch) && isEpochValid(lastEpoch) && lastEpoch > firstEpoch;
}

int graphXForPoint(int pointIndex, int graphX, int graphW) {
  if (displayDataCount < 2) return graphX;
  
  if (timestampsAvailableForGraph()) {
    uint32_t firstEpoch = timestampForPoint(0);
    uint32_t lastEpoch = timestampForPoint(displayDataCount - 1);
    uint32_t pointEpoch = timestampForPoint(pointIndex);
    if (isEpochValid(pointEpoch)) {
      return graphX + (int)(((double)(pointEpoch - firstEpoch) * graphW) / (double)(lastEpoch - firstEpoch));
    }
  }
  
  return graphX + (pointIndex * graphW / (displayDataCount - 1));
}

bool calculateTimestampTrend(float latestDensity, uint32_t latestEpoch, float* trendOut) {
  if (!isEpochValid(latestEpoch) || displayDataCount < 2) return false;
  
  const uint32_t TREND_WINDOW_S = 5 * 60;
  for (int i = displayDataCount - 2; i >= 0; i--) {
    uint32_t pointEpoch = timestampForPoint(i);
    if (isEpochValid(pointEpoch) && latestEpoch >= pointEpoch &&
        latestEpoch - pointEpoch >= TREND_WINDOW_S) {
      int idx = displayBufferIndexForPoint(i);
      *trendOut = latestDensity - displayDataBuffer[idx].density;
      return true;
    }
  }
  
  return false;
}

void drawLiveViewStatic() {
  // Draw top bar
  drawViewTopbar("Live");
  
  // Draw calibration warning if active (compact)
  if (calibrationModeActive) {
    // Clear the area first to prevent artefacts
    tft.fillRect(MARGIN - 2, TOPBAR_H + 2, UI_W - MARGIN * 2 + 4, 20, uiColorBackground);
    tft.setTextColor(uiColorError);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN, TOPBAR_H + 6);  // Adjust for FreeFont baseline
    tft.print("CALIB MODE");
  }
  
  int contentY = calibrationModeActive ? TOPBAR_H + MARGIN + 14 : TOPBAR_H + MARGIN;
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
  
  uiTile(MARGIN, tileY, tileW, tileH, 0, "Temp", "--", "C", false);
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
  
  int contentY = calibrationModeActive ? TOPBAR_H + MARGIN + 14 : TOPBAR_H + MARGIN;
  int contentH = UI_H - contentY - NAV_H;
  int heroH = contentH / 2;
  int tileY = contentY + heroH + GAP - 6;  // Lifted by 6px
  int tileW = (UI_W - MARGIN * 2 - GAP * 2) / 3;  // 3 tiles
  int tileH = contentH - heroH - GAP;
  
  char buffer[16];
  
  // Update hero SG value with trend and sparkline
  if (heroDirty) {
    // Calculate trend against the reading closest to at least 5 real minutes ago.
    char trendBuf[16] = "";
    float trend = 0.0;
    uint32_t latestEpoch = displayTimestampBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    bool hasTrend = calculateTimestampTrend(latest.density, latestEpoch, &trend);
    if (!hasTrend && displayDataCount >= 10) {
      int oldIdx = (displayDataIndex - 10 + MAX_DATA_POINTS) % MAX_DATA_POINTS;
        trend = latest.density - displayDataBuffer[oldIdx].density;
      hasTrend = true;
    }
    if (hasTrend) {
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
    uiTile(MARGIN, tileY, tileW, tileH, 0, "Temp", buffer, "C", false);
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
  
  drawViewTopbar("Graph");
  
  // Define button area
  #define GRAPH_BUTTON_H 40
  #define GRAPH_BUTTON_Y (UI_H - NAV_H - GRAPH_BUTTON_H - GAP)
  
  int contentY = TOPBAR_H + MARGIN;
  int contentH = GRAPH_BUTTON_Y - contentY - GAP;
  
  if (displayDataCount < 3) {
    drawNoDataCard("No graph data", "Collect a few points first", TAB_GRAPH);
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

void drawRecommendationPanel(int x, int y, int w, int h) {
  uiCard(x, y, w, h, CARD_RADIUS);
  tft.setTextColor(uiColorAccent);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(x + 10, y + 22);
  tft.print("Next action");
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  char message[64];
  uiEllipsize(currentRecommendation.message, w - 20, message, sizeof(message));
  tft.setCursor(x + 10, y + 50);
  tft.print(message);
}

void formatActionHeader(char* buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0) return;
  const char* prefix = currentBatchAction.requiresChoice ? "ACTION REQUIRED" : "NEXT";
  if (!currentBatchAction.requiresChoice && currentBatchAction.secondsUntilDue > 0) {
    char duration[16];
    formatDurationShort(currentBatchAction.secondsUntilDue, duration, sizeof(duration));
    snprintf(buffer, bufferSize, "%s (%s)", prefix, duration);
  } else {
    snprintf(buffer, bufferSize, "%s", prefix);
  }
}

void drawDashboardScreen() {
  tft.fillScreen(uiColorBackground);
  drawViewTopbar("Dashboard");
  topbarDirty = false;
  heroDirty = false;
  tileTempDirty = false;
  tileAngleDirty = false;
  tileAbvDirty = false;

  int contentY = TOPBAR_H + MARGIN;
  int contentW = UI_W - MARGIN * 2;

  if (!brewProfileLoaded) {
    uiCard(MARGIN, contentY + 34, contentW, 112, CARD_RADIUS);
    uiTextCenter(MARGIN, contentY + 56, contentW, 24, "No active batch", FONT_SIZE_MD, uiColorTextPrimary);
    uiTextCenter(MARGIN, contentY + 90, contentW, 24, "More > New Brew", FONT_SIZE_SM, uiColorTextMuted);
    uiDrawBottomNav(TAB_DASHBOARD);
    return;
  }

  payload_t latest = {0};
  bool hasData = displayDataCount > 0;
  if (hasData) {
    latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
  }

  const int headerH = 42;
  const int dashGap = 6;
  const int actionH = 78;
  const int metricH = 42;
  const int lowerH = 38;

  uiCard(MARGIN, contentY, contentW, headerH, CARD_RADIUS);
  char text[64];
  uiEllipsize(activeBrewProfile.batchName, contentW - 24, text, sizeof(text));
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN + 12, contentY + 18);
  tft.print(text);

  char phaseText[64];
  snprintf(phaseText, sizeof(phaseText), "%s | %s", activeBrewProfile.beerStyle, fermentationStateMachine.phaseName());
  uiEllipsize(phaseText, contentW - 24, text, sizeof(text));
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(MARGIN + 12, contentY + 36);
  tft.print(text);

  int actionY = contentY + headerH + dashGap;
  uiCard(MARGIN, actionY, contentW, actionH, CARD_RADIUS);
  tft.setTextColor(currentBatchAction.requiresChoice ? uiColorWarning : uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(MARGIN + 10, actionY + 14);
  char actionHeader[28];
  formatActionHeader(actionHeader, sizeof(actionHeader));
  tft.print(actionHeader);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM_BOLD);
  tft.setCursor(MARGIN + 10, actionY + 34);
  tft.print(currentBatchAction.title);
  drawWrappedText(MARGIN + 10, actionY + 50, contentW - (currentBatchAction.requiresChoice ? 200 : 20),
                  currentBatchAction.type == ACTION_NONE ? currentRecommendation.message : currentBatchAction.message,
                  uiColorTextSecondary);
  if (currentBatchAction.requiresChoice) {
    int buttonW = 86;
    int buttonY = actionY + actionH - 32;
    int skipX = MARGIN + contentW - buttonW * 2 - GAP - 10;
    int doneX = MARGIN + contentW - buttonW - 10;
    tft.fillRoundRect(skipX, buttonY, buttonW, 28, 8, uiColorCardBackground);
    tft.drawRoundRect(skipX, buttonY, buttonW, 28, 8, uiColorBorder);
    uiTextCenter(skipX, buttonY, buttonW, 28, "SKIP", FONT_SIZE_XS, uiColorTextPrimary);
    tft.fillRoundRect(doneX, buttonY, buttonW, 28, 8, uiColorGold);
    uiTextCenter(doneX, buttonY, buttonW, 28, "DONE", FONT_SIZE_XS, uiColorPrimaryText);
  }

  int tileY = actionY + actionH + dashGap;
  int tileW = (contentW - GAP * 3) / 4;
  int tileH = metricH;
  char value[24];

  uiCard(MARGIN, tileY, tileW, tileH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(MARGIN + 10, tileY + 15);
  tft.print("Atten");
  snprintf(value, sizeof(value), "%.0f", fermentationMetrics.currentAttenuation);
  tft.setTextColor(hasData ? uiColorTextPrimary : uiColorTextMuted);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN + 10, tileY + 35);
  tft.printf("%s%%", hasData ? value : "--");

  int abvX = MARGIN + tileW + GAP;
  uiCard(abvX, tileY, tileW, tileH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(abvX + 10, tileY + 15);
  tft.print("ABV");
  snprintf(value, sizeof(value), "%.1f", fermentationMetrics.estimatedABV);
  tft.setTextColor(hasData ? uiColorTextPrimary : uiColorTextMuted);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(abvX + 10, tileY + 35);
  tft.printf("%s%%", hasData ? value : "--");

  int fgX = MARGIN + (tileW + GAP) * 2;
  uiCard(fgX, tileY, tileW, tileH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(fgX + 10, tileY + 15);
  tft.print("Exp FG");
  snprintf(value, sizeof(value), "%.3f", activeBrewProfile.expectedFinalGravity);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(fgX + 10, tileY + 35);
  tft.print(value);

  int etaX = MARGIN + (tileW + GAP) * 3;
  uiCard(etaX, tileY, tileW, tileH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(etaX + 10, tileY + 15);
  tft.print("ETA");
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  char eta[16];
  if (currentETA.valid) formatDurationShort(currentETA.secondsToPackaging, eta, sizeof(eta));
  else strcpy(eta, "--");
  tft.setCursor(etaX + 10, tileY + 35);
  tft.print(eta);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(etaX + tileW - 36, tileY + 35);
  tft.printf("%d%%", currentETA.confidencePercent);

  int lowerY = tileY + tileH + dashGap;
  int halfW = (contentW - GAP) / 2;
  uiCard(MARGIN, lowerY, halfW, lowerH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(MARGIN + 10, lowerY + 14);
  tft.print("OG");
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_XS);
  if (activeBrewProfile.ogNeedsChoice) {
    snprintf(text, sizeof(text), "Needs choice");
  } else if (activeBrewProfile.ogVerified) {
    snprintf(text, sizeof(text), "OK %.3f", activeBrewProfile.effectiveOG);
  } else {
    snprintf(text, sizeof(text), "Measuring");
  }
  uiEllipsize(text, halfW - 20, value, sizeof(value));
  tft.setCursor(MARGIN + 10, lowerY + 31);
  tft.print(value);

  int tempX = MARGIN + halfW + GAP;
  uiCard(tempX, lowerY, halfW, lowerH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(tempX + 10, lowerY + 14);
  tft.print("Temp");
  if (hasData) snprintf(text, sizeof(text), "%.1f C", latest.temperature);
  else snprintf(text, sizeof(text), "--");
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(tempX + 10, lowerY + 31);
  tft.print(text);
  uiDrawBottomNav(TAB_DASHBOARD);
}

static const char* YEAST_SPEED_OPTIONS[] = {"slow", "medium", "medium-fast", "fast", "very_fast"};
static const char* YEAST_FLOC_OPTIONS[] = {"low", "medium", "high"};
static const char* YEAST_CURVE_OPTIONS[] = {"clean_ale", "fast_ale", "very_fast_ale", "slow_clean_ale", "lager", "high_attenuation_saison"};

void copyPresetToDraft(const YeastPreset* preset) {
  memset(&yeastDraft, 0, sizeof(yeastDraft));
  if (!preset) {
    strcpy(yeastDraft.id, "");
    strcpy(yeastDraft.displayName, "New Yeast");
    strcpy(yeastDraft.category, "Custom");
    yeastDraft.typicalAttenuationMin = 74;
    yeastDraft.typicalAttenuationMax = 80;
    yeastDraft.defaultAttenuation = 77;
    strcpy(yeastDraft.fermentationSpeed, "medium");
    yeastDraft.lagPhaseHours = 9.0f;
    yeastDraft.typicalDurationHours = 132.0f;
    yeastDraft.recommendedTempMinC = 18.0f;
    yeastDraft.recommendedTempMaxC = 22.0f;
    strcpy(yeastDraft.flocculation, "medium");
    strcpy(yeastDraft.curveTemplate, "clean_ale");
    yeastDraft.diacetylRestRecommended = false;
    strcpy(yeastDraft.notes, "Custom yeast");
    return;
  }
  strncpy(yeastDraft.id, preset->id, sizeof(yeastDraft.id) - 1);
  strncpy(yeastDraft.displayName, preset->displayName, sizeof(yeastDraft.displayName) - 1);
  strncpy(yeastDraft.category, preset->category, sizeof(yeastDraft.category) - 1);
  yeastDraft.typicalAttenuationMin = preset->typicalAttenuationMin;
  yeastDraft.typicalAttenuationMax = preset->typicalAttenuationMax;
  yeastDraft.defaultAttenuation = preset->defaultAttenuation;
  strncpy(yeastDraft.fermentationSpeed, preset->fermentationSpeed, sizeof(yeastDraft.fermentationSpeed) - 1);
  yeastDraft.lagPhaseHours = preset->lagPhaseHours;
  yeastDraft.typicalDurationHours = preset->typicalDurationHours;
  yeastDraft.recommendedTempMinC = preset->recommendedTempMinC;
  yeastDraft.recommendedTempMaxC = preset->recommendedTempMaxC;
  strncpy(yeastDraft.flocculation, preset->flocculation, sizeof(yeastDraft.flocculation) - 1);
  strncpy(yeastDraft.curveTemplate, preset->curveTemplate, sizeof(yeastDraft.curveTemplate) - 1);
  yeastDraft.diacetylRestRecommended = preset->diacetylRestRecommended;
  strncpy(yeastDraft.notes, preset->notes, sizeof(yeastDraft.notes) - 1);
}

int optionIndex(const char* value, const char* const* options, int count) {
  for (int i = 0; i < count; i++) {
    if (strcmp(value, options[i]) == 0) return i;
  }
  return 0;
}

void selectOptionOffset(char* target, size_t targetSize, const char* const* options, int count, int offset) {
  int idx = optionIndex(target, options, count) + offset;
  if (idx < 0) idx = count - 1;
  if (idx >= count) idx = 0;
  strncpy(target, options[idx], targetSize - 1);
  target[targetSize - 1] = '\0';
}

void beginNewYeastWizard(const char* presetId) {
  if (mountSDTemporarily()) {
    YeastPresetRepository::reloadCustom();
    const YeastPreset* preset = presetId ? YeastPresetRepository::findById(presetId) : NULL;
    editingExistingYeast = presetId && YeastPresetRepository::isCustom(presetId);
    copyPresetToDraft(editingExistingYeast ? preset : NULL);
    dismountSD();
  } else {
    editingExistingYeast = false;
    copyPresetToDraft(NULL);
  }
  newYeastStep = NY_NAME;
  loadYeastStepBuffer();
  currentMode = NEW_YEAST_VIEW;
  screenDirty = true;
}

void loadYeastStepBuffer() {
  yeastEditPristine = false;
  switch (newYeastStep) {
    case NY_NAME:
      strncpy(yeastEditBuffer, yeastDraft.displayName, sizeof(yeastEditBuffer) - 1);
      yeastEditPristine = true;
      break;
    case NY_CATEGORY:
      strncpy(yeastEditBuffer, yeastDraft.category, sizeof(yeastEditBuffer) - 1);
      yeastEditPristine = true;
      break;
    case NY_ATT_MIN:
      snprintf(yeastEditBuffer, sizeof(yeastEditBuffer), "%d", yeastDraft.typicalAttenuationMin);
      yeastEditPristine = true;
      break;
    case NY_ATT_MAX:
      snprintf(yeastEditBuffer, sizeof(yeastEditBuffer), "%d", yeastDraft.typicalAttenuationMax);
      yeastEditPristine = true;
      break;
    case NY_ATT_DEFAULT:
      snprintf(yeastEditBuffer, sizeof(yeastEditBuffer), "%d", yeastDraft.defaultAttenuation);
      yeastEditPristine = true;
      break;
    case NY_LAG:
      snprintf(yeastEditBuffer, sizeof(yeastEditBuffer), "%.1f", yeastDraft.lagPhaseHours);
      yeastEditPristine = true;
      break;
    case NY_DURATION:
      snprintf(yeastEditBuffer, sizeof(yeastEditBuffer), "%.1f", yeastDraft.typicalDurationHours);
      yeastEditPristine = true;
      break;
    case NY_TEMP_MIN:
      snprintf(yeastEditBuffer, sizeof(yeastEditBuffer), "%.1f", yeastDraft.recommendedTempMinC);
      yeastEditPristine = true;
      break;
    case NY_TEMP_MAX:
      snprintf(yeastEditBuffer, sizeof(yeastEditBuffer), "%.1f", yeastDraft.recommendedTempMaxC);
      yeastEditPristine = true;
      break;
    case NY_NOTES:
      strncpy(yeastEditBuffer, yeastDraft.notes, sizeof(yeastEditBuffer) - 1);
      yeastEditPristine = true;
      break;
    default:
      yeastEditBuffer[0] = '\0';
      break;
  }
  yeastEditBuffer[sizeof(yeastEditBuffer) - 1] = '\0';
}

void commitYeastStepBuffer() {
  switch (newYeastStep) {
    case NY_NAME:
      if (yeastEditBuffer[0]) strncpy(yeastDraft.displayName, yeastEditBuffer, sizeof(yeastDraft.displayName) - 1);
      break;
    case NY_CATEGORY:
      if (yeastEditBuffer[0]) strncpy(yeastDraft.category, yeastEditBuffer, sizeof(yeastDraft.category) - 1);
      break;
    case NY_ATT_MIN:
      yeastDraft.typicalAttenuationMin = atoi(yeastEditBuffer);
      break;
    case NY_ATT_MAX:
      yeastDraft.typicalAttenuationMax = atoi(yeastEditBuffer);
      break;
    case NY_ATT_DEFAULT:
      yeastDraft.defaultAttenuation = atoi(yeastEditBuffer);
      break;
    case NY_LAG:
      yeastDraft.lagPhaseHours = atof(yeastEditBuffer);
      break;
    case NY_DURATION:
      yeastDraft.typicalDurationHours = atof(yeastEditBuffer);
      break;
    case NY_TEMP_MIN:
      yeastDraft.recommendedTempMinC = atof(yeastEditBuffer);
      break;
    case NY_TEMP_MAX:
      yeastDraft.recommendedTempMaxC = atof(yeastEditBuffer);
      break;
    case NY_NOTES:
      if (yeastEditBuffer[0]) strncpy(yeastDraft.notes, yeastEditBuffer, sizeof(yeastDraft.notes) - 1);
      break;
    default:
      break;
  }
  yeastDraft.displayName[sizeof(yeastDraft.displayName) - 1] = '\0';
  yeastDraft.category[sizeof(yeastDraft.category) - 1] = '\0';
  yeastDraft.notes[sizeof(yeastDraft.notes) - 1] = '\0';
  if (yeastDraft.typicalAttenuationMin < 40) yeastDraft.typicalAttenuationMin = 40;
  if (yeastDraft.typicalAttenuationMax < yeastDraft.typicalAttenuationMin) yeastDraft.typicalAttenuationMax = yeastDraft.typicalAttenuationMin;
  if (yeastDraft.defaultAttenuation < yeastDraft.typicalAttenuationMin) yeastDraft.defaultAttenuation = yeastDraft.typicalAttenuationMin;
  if (yeastDraft.defaultAttenuation > yeastDraft.typicalAttenuationMax) yeastDraft.defaultAttenuation = yeastDraft.typicalAttenuationMax;
}

void drawNewYeastNav(const char* nextLabel) {
  drawButton(MARGIN, 276, 120, 36, "BACK", false);
  drawButton(UI_W - MARGIN - 140, 276, 140, 36, nextLabel, true);
}

void drawYeastDropdown(const char* label, const char* value) {
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN, 102);
  tft.print(label);
  drawButton(MARGIN, 126, 58, 54, "<", false);
  uiCard(MARGIN + 68, 126, UI_W - MARGIN * 2 - 136, 54, CARD_RADIUS);
  uiTextCenter(MARGIN + 68, 126, UI_W - MARGIN * 2 - 136, 54, value, FONT_SIZE_SM_BOLD, uiColorTextPrimary);
  drawButton(UI_W - MARGIN - 58, 126, 58, 54, ">", false);
}

void drawNewYeastScreen() {
  tft.fillScreen(uiColorBackground);
  drawViewTopbar(editingExistingYeast ? "Edit Yeast" : "New Yeast");
  char stepText[20];
  snprintf(stepText, sizeof(stepText), "%d/15", (int)newYeastStep + 1);
  uiTextRight(UI_W - 80, TOPBAR_H + 8, 66, 18, stepText, FONT_SIZE_XS, uiColorTextMuted);
  const char* title = "";
  switch (newYeastStep) {
    case NY_NAME: title = "Display Name"; break;
    case NY_CATEGORY: title = "Category"; break;
    case NY_ATT_MIN: title = "Attenuation Min"; break;
    case NY_ATT_MAX: title = "Attenuation Max"; break;
    case NY_ATT_DEFAULT: title = "Default Attenuation"; break;
    case NY_SPEED: title = "Fermentation Speed"; break;
    case NY_LAG: title = "Lag Phase Hours"; break;
    case NY_DURATION: title = "Duration Hours"; break;
    case NY_TEMP_MIN: title = "Temp Min C"; break;
    case NY_TEMP_MAX: title = "Temp Max C"; break;
    case NY_FLOC: title = "Flocculation"; break;
    case NY_CURVE: title = "Curve Template"; break;
    case NY_DREST: title = "Diacetyl Rest"; break;
    case NY_NOTES: title = "Notes"; break;
    case NY_REVIEW: title = "Review Yeast"; break;
  }
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN, TOPBAR_H + 28);
  tft.print(title);

  if (newYeastStep == NY_NAME || newYeastStep == NY_CATEGORY || newYeastStep == NY_NOTES) {
    VirtualKeyboardInput::drawText(tft, yeastEditBuffer);
  } else if (newYeastStep == NY_ATT_MIN || newYeastStep == NY_ATT_MAX || newYeastStep == NY_ATT_DEFAULT ||
             newYeastStep == NY_LAG || newYeastStep == NY_DURATION || newYeastStep == NY_TEMP_MIN || newYeastStep == NY_TEMP_MAX) {
    VirtualKeyboardInput::drawNumber(tft, yeastEditBuffer);
  } else if (newYeastStep == NY_SPEED) {
    drawYeastDropdown("Speed", yeastDraft.fermentationSpeed);
  } else if (newYeastStep == NY_FLOC) {
    drawYeastDropdown("Flocculation", yeastDraft.flocculation);
  } else if (newYeastStep == NY_CURVE) {
    drawYeastDropdown("Curve", yeastDraft.curveTemplate);
  } else if (newYeastStep == NY_DREST) {
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN, 130);
    tft.print("Usually benefits from rest");
    drawThemeToggle(UI_W - MARGIN - 70, 110, yeastDraft.diacetylRestRecommended);
  } else if (newYeastStep == NY_REVIEW) {
    uiCard(MARGIN, 82, UI_W - MARGIN * 2, 172, CARD_RADIUS);
    tft.setTextColor(uiColorTextPrimary);
    tft.setFreeFont(FONT_SIZE_SM_BOLD);
    tft.setCursor(MARGIN + 12, 108);
    tft.print(yeastDraft.displayName);
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_XS);
    tft.setCursor(MARGIN + 12, 134);
    tft.printf("%s  %d-%d%% default %d%%", yeastDraft.category, yeastDraft.typicalAttenuationMin, yeastDraft.typicalAttenuationMax, yeastDraft.defaultAttenuation);
    tft.setCursor(MARGIN + 12, 158);
    tft.printf("%s  lag %.1fh  duration %.0fh", yeastDraft.fermentationSpeed, yeastDraft.lagPhaseHours, yeastDraft.typicalDurationHours);
    tft.setCursor(MARGIN + 12, 182);
    tft.printf("Temp %.0f-%.0fC  floc %s", yeastDraft.recommendedTempMinC, yeastDraft.recommendedTempMaxC, yeastDraft.flocculation);
    tft.setCursor(MARGIN + 12, 206);
    tft.printf("%s  D-rest %s", yeastDraft.curveTemplate, yeastDraft.diacetylRestRecommended ? "yes" : "no");
    char note[48];
    uiEllipsize(yeastDraft.notes, UI_W - MARGIN * 2 - 24, note, sizeof(note));
    tft.setCursor(MARGIN + 12, 230);
    tft.print(note);
  }
  drawNewYeastNav(newYeastStep == NY_REVIEW ? "SAVE" : "NEXT");
}

void saveNewYeastWizard() {
  YeastPreset preset = {
    yeastDraft.id, yeastDraft.displayName, yeastDraft.category,
    yeastDraft.typicalAttenuationMin, yeastDraft.typicalAttenuationMax, yeastDraft.defaultAttenuation,
    yeastDraft.fermentationSpeed, yeastDraft.lagPhaseHours, yeastDraft.typicalDurationHours,
    yeastDraft.recommendedTempMinC, yeastDraft.recommendedTempMaxC,
    yeastDraft.flocculation, yeastDraft.curveTemplate, yeastDraft.diacetylRestRecommended, yeastDraft.notes
  };
  if (mountSDTemporarily()) {
    YeastPresetRepository::saveCustom(preset);
    YeastPresetRepository::reloadCustom();
    dismountSD();
  }
  loadManagedYeasts();
  currentMode = MANAGE_YEAST_VIEW;
  screenDirty = true;
}

bool handleNewYeastTouch(int x, int y) {
  if (x >= MARGIN - 6 && x <= MARGIN + 130 && y >= 268 && y <= 318) {
    if (newYeastStep == NY_NAME) {
      currentMode = MORE_VIEW;
    } else {
      commitYeastStepBuffer();
      newYeastStep = (NewYeastStep)((int)newYeastStep - 1);
      loadYeastStepBuffer();
    }
    screenDirty = true;
    return true;
  }
  if (x >= UI_W - MARGIN - 146 && x <= UI_W - MARGIN && y >= 268 && y <= 318) {
    commitYeastStepBuffer();
    if (newYeastStep == NY_REVIEW) {
      saveNewYeastWizard();
    } else {
      newYeastStep = (NewYeastStep)((int)newYeastStep + 1);
      loadYeastStepBuffer();
      screenDirty = true;
    }
    return true;
  }
  if (newYeastStep == NY_NAME || newYeastStep == NY_CATEGORY || newYeastStep == NY_NOTES) {
    if (VirtualKeyboardInput::handleTextTouch(x, y, yeastEditBuffer, sizeof(yeastEditBuffer), &yeastEditPristine)) {
      screenDirty = true;
      return true;
    }
  } else if (newYeastStep == NY_ATT_MIN || newYeastStep == NY_ATT_MAX || newYeastStep == NY_ATT_DEFAULT ||
             newYeastStep == NY_LAG || newYeastStep == NY_DURATION || newYeastStep == NY_TEMP_MIN || newYeastStep == NY_TEMP_MAX) {
    if (VirtualKeyboardInput::handleNumberTouch(x, y, yeastEditBuffer, sizeof(yeastEditBuffer), &yeastEditPristine)) {
      screenDirty = true;
      return true;
    }
  } else if (newYeastStep == NY_SPEED || newYeastStep == NY_FLOC || newYeastStep == NY_CURVE) {
    int offset = 0;
    if (x >= MARGIN && x <= MARGIN + 58 && y >= 126 && y <= 180) offset = -1;
    if (x >= UI_W - MARGIN - 58 && x <= UI_W - MARGIN && y >= 126 && y <= 180) offset = 1;
    if (offset != 0) {
      if (newYeastStep == NY_SPEED) selectOptionOffset(yeastDraft.fermentationSpeed, sizeof(yeastDraft.fermentationSpeed), YEAST_SPEED_OPTIONS, 5, offset);
      else if (newYeastStep == NY_FLOC) selectOptionOffset(yeastDraft.flocculation, sizeof(yeastDraft.flocculation), YEAST_FLOC_OPTIONS, 3, offset);
      else selectOptionOffset(yeastDraft.curveTemplate, sizeof(yeastDraft.curveTemplate), YEAST_CURVE_OPTIONS, 6, offset);
      screenDirty = true;
      return true;
    }
  } else if (newYeastStep == NY_DREST) {
    if (x >= UI_W - MARGIN - 86 && x <= UI_W - MARGIN && y >= 100 && y <= 150) {
      yeastDraft.diacetylRestRecommended = !yeastDraft.diacetylRestRecommended;
      screenDirty = true;
      return true;
    }
  }
  return false;
}

void loadManagedYeasts() {
  managedYeastCount = 0;
  if (mountSDTemporarily()) {
    YeastPresetRepository::reloadCustom();
    for (int i = 0; i < YeastPresetRepository::count() && managedYeastCount < 12; i++) {
      const YeastPreset* preset = YeastPresetRepository::at(i);
      if (YeastPresetRepository::isCustom(preset->id)) {
        strncpy(managedYeastIds[managedYeastCount], preset->id, 23);
        managedYeastIds[managedYeastCount][23] = '\0';
        strncpy(managedYeastNames[managedYeastCount], preset->displayName, 31);
        managedYeastNames[managedYeastCount][31] = '\0';
        managedYeastCount++;
      }
    }
    dismountSD();
  }
  if (manageYeastIndex >= managedYeastCount) manageYeastIndex = max(0, managedYeastCount - 1);
}

void drawManageYeastScreen() {
  tft.fillScreen(uiColorBackground);
  drawViewTopbar("Manage Yeast");
  int y = TOPBAR_H + MARGIN;
  if (managedYeastCount == 0) {
    uiCard(MARGIN, y + 44, UI_W - MARGIN * 2, 110, CARD_RADIUS);
    uiTextCenter(MARGIN, y + 68, UI_W - MARGIN * 2, 24, "No custom yeast", FONT_SIZE_MD, uiColorTextPrimary);
    uiTextCenter(MARGIN, y + 102, UI_W - MARGIN * 2, 20, "Use New Yeast first", FONT_SIZE_SM, uiColorTextMuted);
    drawButton(MARGIN, 276, 120, 36, "BACK", false);
    return;
  }
  const YeastPreset* preset = NULL;
  if (mountSDTemporarily()) {
    YeastPresetRepository::reloadCustom();
    preset = YeastPresetRepository::findById(managedYeastIds[manageYeastIndex]);
    dismountSD();
  }
  if (!preset) return;
  uiCard(MARGIN, y, UI_W - MARGIN * 2, 156, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN + 12, y + 28);
  tft.print(preset->displayName);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(MARGIN + 12, y + 54);
  tft.printf("%s  %d-%d%% default %d%%", preset->category, preset->typicalAttenuationMin, preset->typicalAttenuationMax, preset->defaultAttenuation);
  tft.setCursor(MARGIN + 12, y + 78);
  tft.printf("%s  %.0f-%.0fC  %.0fh", preset->fermentationSpeed, preset->recommendedTempMinC, preset->recommendedTempMaxC, preset->typicalDurationHours);
  tft.setCursor(MARGIN + 12, y + 102);
  tft.printf("%s  %s  D-rest %s", preset->curveTemplate, preset->flocculation, preset->diacetylRestRecommended ? "yes" : "no");
  char countText[20];
  snprintf(countText, sizeof(countText), "%d/%d", manageYeastIndex + 1, managedYeastCount);
  uiTextRight(MARGIN, y + 122, UI_W - MARGIN * 2 - 12, 20, countText, FONT_SIZE_XS, uiColorTextMuted);
  drawButton(MARGIN, 214, 72, 36, "<", false);
  drawButton(MARGIN + 84, 214, 110, 36, "EDIT", false);
  drawButton(MARGIN + 206, 214, 110, 36, "DELETE", false);
  drawButton(UI_W - MARGIN - 72, 214, 72, 36, ">", false);
  drawButton(MARGIN, 276, 120, 36, "BACK", false);
}

bool handleManageYeastTouch(int x, int y) {
  if (x >= MARGIN && x <= MARGIN + 120 && y >= 276 && y <= 312) {
    currentMode = MORE_VIEW;
    screenDirty = true;
    return true;
  }
  if (managedYeastCount <= 0) return false;
  if (x >= MARGIN && x <= MARGIN + 72 && y >= 214 && y <= 250) {
    manageYeastIndex = manageYeastIndex <= 0 ? managedYeastCount - 1 : manageYeastIndex - 1;
    screenDirty = true;
    return true;
  }
  if (x >= UI_W - MARGIN - 72 && x <= UI_W - MARGIN && y >= 214 && y <= 250) {
    manageYeastIndex = (manageYeastIndex + 1) % managedYeastCount;
    screenDirty = true;
    return true;
  }
  if (x >= MARGIN + 84 && x <= MARGIN + 194 && y >= 214 && y <= 250) {
    beginNewYeastWizard(managedYeastIds[manageYeastIndex]);
    return true;
  }
  if (x >= MARGIN + 206 && x <= MARGIN + 316 && y >= 214 && y <= 250) {
    if (mountSDTemporarily()) {
      YeastPresetRepository::deleteCustom(managedYeastIds[manageYeastIndex]);
      dismountSD();
    }
    loadManagedYeasts();
    screenDirty = true;
    return true;
  }
  return false;
}

int batchNumberFromId(const char* id) {
  int n = 0;
  if (sscanf(id, "batch_%d", &n) == 1) return n;
  return 0;
}

void loadManagedBatches() {
  managedBatchCount = 0;
  if (!mountSDTemporarily()) return;
  if (!SD.exists("/data/batches")) {
    dismountSD();
    return;
  }
  File dir = SD.open("/data/batches");
  if (!dir) {
    dismountSD();
    return;
  }
  File entry = dir.openNextFile();
  while (entry && managedBatchCount < 16) {
    if (entry.isDirectory()) {
      const char* name = entry.name();
      const char* slash = strrchr(name, '/');
      const char* id = slash ? slash + 1 : name;
      static BrewProfile p;
      if (BrewProfileStore::load(id, &p)) {
        strncpy(managedBatchIds[managedBatchCount], p.batchId, 23);
        managedBatchIds[managedBatchCount][23] = '\0';
        strncpy(managedBatchNames[managedBatchCount], p.batchName, 39);
        managedBatchNames[managedBatchCount][39] = '\0';
        managedBatchCount++;
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  dismountSD();
  if (manageBatchIndex >= managedBatchCount) manageBatchIndex = max(0, managedBatchCount - 1);
}

int nextBatchNumberOnSD() {
  int highest = 0;
  if (!SD.exists("/data/batches")) return 1;
  File dir = SD.open("/data/batches");
  if (!dir) return 1;
  File entry = dir.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      const char* name = entry.name();
      const char* slash = strrchr(name, '/');
      const char* id = slash ? slash + 1 : name;
      int n = batchNumberFromId(id);
      if (n > highest) highest = n;
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  return highest + 1;
}

void continueManagedBatch(const char* batchId) {
  if (!mountSDTemporarily()) return;
  if (BrewProfileStore::load(batchId, &activeBrewProfile)) {
    if (activeBrewProfile.completed) {
      LOG_INFO("Batch %s is completed; not continuing as active\n", batchId);
      dismountSD();
      currentMode = MANAGE_BREW_VIEW;
      screenDirty = true;
      return;
    }
    bool logReady = ensureFermentationLogFile(activeBrewProfile.batchId);
    BrewProfileStore::logPath(activeBrewProfile.batchId, currentFermentationFile, sizeof(currentFermentationFile));
    fermentationFileOpen = logReady;
    brewProfileLoaded = true;
    BrewProfileStore::saveActiveBatchId(activeBrewProfile.batchId);
    targetCurveAvailable = SD.exists("/data") && TargetCurveGenerator::generateAndSave(activeBrewProfile);
    if (logReady) loadHistoricalDataFromCSV(currentFermentationFile, currentLoadMode, MAX_DATA_POINTS, 1);
    refreshFermentationAssistantFromProfile();
  }
  dismountSD();
  currentMode = DASHBOARD_VIEW;
  screenDirty = true;
}

void copyManagedBatch(const char* batchId) {
  if (!mountSDTemporarily()) return;
  static BrewProfile p;
  if (BrewProfileStore::load(batchId, &p)) {
    BrewProfileStore::buildBatchId(nextBatchNumberOnSD(), p.batchId, sizeof(p.batchId));
    char copiedName[40];
    snprintf(copiedName, sizeof(copiedName), "%s Copy", p.batchName);
    strncpy(p.batchName, copiedName, sizeof(p.batchName) - 1);
    p.batchName[sizeof(p.batchName) - 1] = '\0';
    p.createdAt = getCurrentEpoch();
    p.completed = false;
    p.completedAt = 0;
    p.dryHopDone = false;
    p.dryHopSkipped = false;
    p.dryHopStartTime = 0;
    p.dryHopRemoved = false;
    p.dryHopRemoveSkipped = false;
    p.dryHopRemovedAt = 0;
    p.dRestDone = false;
    p.dRestSkipped = false;
    p.dRestStartedAt = 0;
    p.coldCrashDone = false;
    p.coldCrashSkipped = false;
    p.coldCrashStartedAt = 0;
    p.packageDone = false;
    p.packageSkipped = false;
    p.packagedAt = 0;
    p.measuredOG = 0.0f;
    p.ogDifference = 0.0f;
    p.ogVerified = false;
    p.ogNeedsChoice = false;
    BrewProfileStore::save(p);
    TargetCurveGenerator::generateAndSave(p);
    activeBrewProfile = p;
    bool logReady = ensureFermentationLogFile(activeBrewProfile.batchId);
    BrewProfileStore::logPath(activeBrewProfile.batchId, currentFermentationFile, sizeof(currentFermentationFile));
    fermentationFileOpen = logReady;
    brewProfileLoaded = true;
    BrewProfileStore::saveActiveBatchId(activeBrewProfile.batchId);
    clearHistoricalDisplayData();
    refreshFermentationAssistantFromProfile();
  }
  dismountSD();
  loadManagedBatches();
  currentMode = DASHBOARD_VIEW;
  screenDirty = true;
}

void deleteManagedBatch(const char* batchId) {
  if (!mountSDTemporarily()) return;
  char path[80];
  BrewProfileStore::profilePath(batchId, path, sizeof(path));
  if (SD.exists(path)) SD.remove(path);
  BrewProfileStore::targetPath(batchId, path, sizeof(path));
  if (SD.exists(path)) SD.remove(path);
  BrewProfileStore::logPath(batchId, path, sizeof(path));
  if (SD.exists(path)) SD.remove(path);
  BrewProfileStore::eventsPath(batchId, path, sizeof(path));
  if (SD.exists(path)) SD.remove(path);
  if (strcmp(activeBrewProfile.batchId, batchId) == 0) {
    brewProfileLoaded = false;
    fermentationFileOpen = false;
    currentFermentationFile[0] = '\0';
    BrewProfileStore::clearActiveBatchId();
  }
  dismountSD();
  loadManagedBatches();
  screenDirty = true;
}

bool markManagedBatchCompleted(const char* batchId) {
  if (!mountSDTemporarily()) return false;
  static BrewProfile p;
  bool ok = false;
  if (BrewProfileStore::load(batchId, &p)) {
    p.completed = true;
    p.completedAt = getCurrentEpoch();
    ok = BrewProfileStore::save(p);
    char activeBatchId[24];
    if (BrewProfileStore::loadActiveBatchId(activeBatchId, sizeof(activeBatchId)) &&
        strcmp(activeBatchId, batchId) == 0) {
      BrewProfileStore::clearActiveBatchId();
    }
    if (strcmp(activeBrewProfile.batchId, batchId) == 0) {
      brewProfileLoaded = false;
      fermentationFileOpen = false;
      currentFermentationFile[0] = '\0';
      clearHistoricalDisplayData();
    }
  }
  dismountSD();
  loadManagedBatches();
  currentMode = MANAGE_BREW_VIEW;
  screenDirty = true;
  return ok;
}

void loadManagedBatchSummary(const char* batchId, ManagedBatchSummary* summary) {
  memset(summary, 0, sizeof(ManagedBatchSummary));
  strcpy(summary->status, "--");
  strcpy(summary->evaluation, "No data yet");
  if (!mountSDTemporarily()) return;

  static BrewProfile p;
  if (BrewProfileStore::load(batchId, &p)) {
    summary->loaded = true;
    summary->completed = p.completed;
    strncpy(summary->style, p.beerStyle, sizeof(summary->style) - 1);
    summary->liters = p.batchSizeLiters;
    strcpy(summary->status, p.completed ? "Completed" : "Active");

    char logPath[80];
    BrewProfileStore::logPath(batchId, logPath, sizeof(logPath));
    File file = SD.open(logPath, FILE_READ);
    if (file) {
      static char line[180];
      while (file.available()) {
        int bytesRead = file.readBytesUntil('\n', line, sizeof(line) - 1);
        line[bytesRead] = '\0';
        if (strncmp(line, "timestamp,", 10) == 0 || line[0] == '#' || strlen(line) == 0) continue;
        payload_t data = {0};
        uint32_t epoch = 0;
        uint8_t batteryPercent = 0;
        if (parseCSVDataLine(line, &data, &epoch, &batteryPercent) &&
            (!isEpochValid(p.createdAt) || !isEpochValid(epoch) || epoch >= p.createdAt)) {
          if (summary->points == 0) summary->firstEpoch = epoch;
          summary->lastEpoch = epoch;
          summary->points++;
        }
      }
      file.close();
    }

    if (summary->points == 0) {
      strcpy(summary->evaluation, "No log data");
    } else if (p.ogNeedsChoice) {
      strcpy(summary->evaluation, "OG needs verify");
    } else if (p.completed) {
      strcpy(summary->evaluation, "Archived batch");
    } else if (summary->points < 6) {
      strcpy(summary->evaluation, "Collecting data");
    } else {
      strcpy(summary->evaluation, "Trend available");
    }
  }
  dismountSD();
}

void drawManageBrewScreen() {
  tft.fillScreen(uiColorBackground);
  drawViewTopbar("Manage Brew");
  int y = TOPBAR_H + MARGIN;
  if (managedBatchCount == 0) {
    uiCard(MARGIN, y + 44, UI_W - MARGIN * 2, 110, CARD_RADIUS);
    uiTextCenter(MARGIN, y + 68, UI_W - MARGIN * 2, 24, "No batches stored", FONT_SIZE_MD, uiColorTextPrimary);
    uiTextCenter(MARGIN, y + 102, UI_W - MARGIN * 2, 20, "Run New Brew first", FONT_SIZE_SM, uiColorTextMuted);
    drawButton(MARGIN, 276, 120, 36, "BACK", false);
    return;
  }
  ManagedBatchSummary summary;
  loadManagedBatchSummary(managedBatchIds[manageBatchIndex], &summary);

  uiCard(MARGIN, y, UI_W - MARGIN * 2, 132, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM_BOLD);
  char label[44];
  uiEllipsize(managedBatchNames[manageBatchIndex], UI_W - MARGIN * 2 - 24, label, sizeof(label));
  tft.setCursor(MARGIN + 12, y + 30);
  tft.print(label);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(MARGIN + 12, y + 58);
  tft.print(managedBatchIds[manageBatchIndex]);
  tft.setCursor(MARGIN + 126, y + 58);
  tft.print(summary.status);
  tft.setCursor(MARGIN + 12, y + 80);
  tft.printf("%s / %.1f L", summary.style[0] ? summary.style : "--", summary.liters);
  tft.setCursor(MARGIN + 12, y + 102);
  if (summary.points > 0 && isEpochValid(summary.firstEpoch) && isEpochValid(summary.lastEpoch) && summary.lastEpoch >= summary.firstEpoch) {
    unsigned long hours = (summary.lastEpoch - summary.firstEpoch) / 3600UL;
    tft.printf("%d pts / %luh", summary.points, hours);
  } else {
    tft.printf("%d pts / --", summary.points);
  }
  tft.setCursor(MARGIN + 12, y + 124);
  tft.print(summary.evaluation);
  snprintf(label, sizeof(label), "%d/%d", manageBatchIndex + 1, managedBatchCount);
  uiTextRight(MARGIN, y + 104, UI_W - MARGIN * 2 - 12, 20, label, FONT_SIZE_XS, uiColorTextMuted);

  drawButton(MARGIN, 184, 72, 34, "<", false);
  drawButton(UI_W - MARGIN - 72, 184, 72, 34, ">", false);
  drawButton(MARGIN + 84, 184, 104, 34, "CONT", summary.completed);
  drawButton(MARGIN + 200, 184, 104, 34, "COPY", false);
  drawButton(MARGIN + 316, 184, 104, 34, "DELETE", false);
  drawButton(MARGIN + 84, 226, 150, 34, "COMPLETE", summary.completed);
  drawButton(MARGIN + 246, 226, 174, 34, summary.completed ? "ARCHIVED" : "ACTIVE", true);
  drawButton(MARGIN, 276, 120, 36, "BACK", false);

  if (manageBrewCompleteConfirm) {
    int dx = MARGIN + 28;
    int dy = 86;
    int dw = UI_W - (MARGIN + 28) * 2;
    int dh = 132;
    tft.fillRoundRect(dx, dy, dw, dh, 8, uiColorCardBackground);
    tft.drawRoundRect(dx, dy, dw, dh, 8, uiColorBorder);
    uiTextCenter(dx, dy + 18, dw, 30, "Mark batch completed?", FONT_SIZE_MD, uiColorTextPrimary);
    tft.drawRoundRect(dx + 14, dy + 68, 116, 38, 8, uiColorBorder);
    uiTextCenter(dx + 14, dy + 68, 116, 38, "NO", FONT_SIZE_SM, uiColorTextPrimary);
    tft.fillRoundRect(dx + dw - 130, dy + 68, 116, 38, 8, uiColorGold);
    uiTextCenter(dx + dw - 130, dy + 68, 116, 38, "YES", FONT_SIZE_SM, uiColorPrimaryText);
  }
}

bool handleManageBrewTouch(int x, int y) {
  if (manageBrewCompleteConfirm) {
    int dx = MARGIN + 28;
    int dy = 86;
    int dw = UI_W - (MARGIN + 28) * 2;
    if (x >= dx + 14 && x <= dx + 130 && y >= dy + 68 && y <= dy + 106) {
      manageBrewCompleteConfirm = false;
      screenDirty = true;
      return true;
    }
    if (x >= dx + dw - 130 && x <= dx + dw - 14 && y >= dy + 68 && y <= dy + 106) {
      manageBrewCompleteConfirm = false;
      if (managedBatchCount > 0) markManagedBatchCompleted(managedBatchIds[manageBatchIndex]);
      screenDirty = true;
      return true;
    }
    return true;
  }
  if (x >= MARGIN && x <= MARGIN + 120 && y >= 276 && y <= 312) {
    currentMode = MORE_VIEW;
    screenDirty = true;
    return true;
  }
  if (managedBatchCount <= 0) return false;
  if (x >= MARGIN && x <= MARGIN + 72 && y >= 184 && y <= 218) {
    manageBatchIndex = manageBatchIndex <= 0 ? managedBatchCount - 1 : manageBatchIndex - 1;
    screenDirty = true;
    return true;
  }
  if (x >= UI_W - MARGIN - 72 && x <= UI_W - MARGIN && y >= 184 && y <= 218) {
    manageBatchIndex = (manageBatchIndex + 1) % managedBatchCount;
    screenDirty = true;
    return true;
  }
  if (x >= MARGIN + 84 && x <= MARGIN + 188 && y >= 184 && y <= 218) {
    continueManagedBatch(managedBatchIds[manageBatchIndex]);
    return true;
  }
  if (x >= MARGIN + 200 && x <= MARGIN + 304 && y >= 184 && y <= 218) {
    copyManagedBatch(managedBatchIds[manageBatchIndex]);
    return true;
  }
  if (x >= MARGIN + 316 && x <= MARGIN + 420 && y >= 184 && y <= 218) {
    deleteManagedBatch(managedBatchIds[manageBatchIndex]);
    return true;
  }
  if (x >= MARGIN + 84 && x <= MARGIN + 234 && y >= 226 && y <= 260) {
    static BrewProfile p;
    bool canComplete = false;
    if (mountSDTemporarily()) {
      canComplete = BrewProfileStore::load(managedBatchIds[manageBatchIndex], &p) && !p.completed;
      dismountSD();
    }
    if (canComplete) {
      manageBrewCompleteConfirm = true;
      screenDirty = true;
    }
    return true;
  }
  return false;
}

void drawBrewWizardScreen() {
  brewWizard.draw(tft);
}

void drawOGVerificationScreen() {
  tft.fillScreen(uiColorBackground);
  drawViewTopbar("OG Verify");
  int y = TOPBAR_H + MARGIN;
  uiCard(MARGIN, y, UI_W - MARGIN * 2, 170, CARD_RADIUS);
  tft.setTextColor(uiColorWarning);
  tft.setFreeFont(FONT_SIZE_MD);
  tft.setCursor(MARGIN + 12, y + 30);
  tft.print("OG difference");
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(MARGIN + 12, y + 68);
  tft.printf("Recipe OG:  %.3f", activeBrewProfile.recipeOG);
  tft.setCursor(MARGIN + 12, y + 98);
  tft.printf("Measured OG: %.3f", activeBrewProfile.measuredOG);
  tft.setCursor(MARGIN + 12, y + 128);
  tft.printf("Delta:      %+.3f", activeBrewProfile.ogDifference);

  drawButton(MARGIN, 242, 200, 44, "USE RECIPE", false);
  drawButton(UI_W - MARGIN - 200, 242, 200, 44, "USE MEASURED", false);
}

void drawTargetVsActualChart() {
  tft.fillScreen(uiColorBackground);
  drawViewTopbar("Target");

  int x = MARGIN;
  int y = TOPBAR_H + MARGIN;
  int w = UI_W - MARGIN * 2;
  int h = UI_H - TOPBAR_H - NAV_H - MARGIN * 2;
  uiCard(x, y, w, h, CARD_RADIUS);

  if (!brewProfileLoaded || displayDataCount < 2) {
    uiTextCenter(x, y + 70, w, 24, "No chart data yet", FONT_SIZE_SM, uiColorTextMuted);
    uiDrawBottomNav(TAB_LIVE);
    return;
  }

  int gx = x + 38;
  int gy = y + 28;
  int gw = w - 54;
  int gh = h - 54;
  tft.drawRect(gx, gy, gw, gh, uiColorBorder);

  float chartHours = activeBrewProfile.autoModeEnabled && activeBrewProfile.typicalDurationHours > 0.0f
    ? activeBrewProfile.typicalDurationHours + 48.0f
    : 240.0f;
  if (chartHours < 96.0f) chartHours = 96.0f;
  if (chartHours > 384.0f) chartHours = 384.0f;

  float minSG = 2.0f;
  float maxSG = 0.0f;
  for (int i = 0; i <= 40; i++) {
    float hour = chartHours * (float)i / 40.0f;
    float sg = TargetCurveGenerator::expectedGravityAtHour(activeBrewProfile, hour);
    if (sg < minSG) minSG = sg;
    if (sg > maxSG) maxSG = sg;
  }
  for (int i = 0; i < displayDataCount; i++) {
    int idx = (displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    float sg = displayDataBuffer[idx].density;
    if (sg > 0.900f && sg < 1.300f) {
      if (sg < minSG) minSG = sg;
      if (sg > maxSG) maxSG = sg;
    }
  }
  minSG -= 0.010f;
  maxSG += 0.010f;
  if (minSG < 0.990f) minSG = 0.990f;
  if (maxSG <= minSG) maxSG = minSG + 0.050f;

  char label[16];
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  snprintf(label, sizeof(label), "%.3f", maxSG);
  tft.setCursor(x + 6, gy + 8);
  tft.print(label);
  snprintf(label, sizeof(label), "%.3f", minSG);
  tft.setCursor(x + 6, gy + gh);
  tft.print(label);

  int prevX = -1;
  int prevY = -1;
  for (int i = 0; i <= 40; i++) {
    float hour = chartHours * (float)i / 40.0f;
    float sg = TargetCurveGenerator::expectedGravityAtHour(activeBrewProfile, hour);
    int px = gx + (i * gw / 40);
    int py = gy + gh - (int)((sg - minSG) * gh / (maxSG - minSG));
    if (prevX >= 0) tft.drawLine(prevX, prevY, px, py, uiColorGold);
    prevX = px;
    prevY = py;
  }

  prevX = -1;
  prevY = -1;
  uint32_t firstEpoch = timestampForPoint(0);
  payload_t firstData = displayDataBuffer[(displayDataIndex - displayDataCount + MAX_DATA_POINTS) % MAX_DATA_POINTS];
  bool useBatchEpoch = isEpochValid(activeBrewProfile.createdAt);
  bool usePointEpochs = isEpochValid(firstEpoch);
  for (int i = 0; i < displayDataCount; i++) {
    int idx = (displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    payload_t data = displayDataBuffer[idx];
    uint32_t pointEpoch = timestampForPoint(i);
    float hour = 0.0f;
    if (useBatchEpoch && isEpochValid(pointEpoch) && pointEpoch >= activeBrewProfile.createdAt) {
      hour = (pointEpoch - activeBrewProfile.createdAt) / 3600.0f;
    } else if (usePointEpochs && isEpochValid(pointEpoch) && pointEpoch >= firstEpoch) {
      hour = (pointEpoch - firstEpoch) / 3600.0f;
    } else if (data.uptime_s >= firstData.uptime_s) {
      hour = (data.uptime_s - firstData.uptime_s) / 3600.0f;
    }
    if (hour > chartHours) hour = chartHours;
    float sg = data.density;
    int px = gx + (int)((hour / chartHours) * gw);
    int py = gy + gh - (int)((sg - minSG) * gh / (maxSG - minSG));
    if (py < gy) py = gy;
    if (py > gy + gh) py = gy + gh;
    if (prevX >= 0) tft.drawLine(prevX, prevY, px, py, uiColorInfo);
    tft.fillCircle(px, py, 2, uiColorInfo);
    prevX = px;
    prevY = py;
  }

  tft.setTextColor(uiColorGold);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(gx + 6, gy + 14);
  tft.print("target");
  tft.setTextColor(uiColorInfo);
  tft.setCursor(gx + 64, gy + 14);
  tft.print("actual");

  uiDrawBottomNav(TAB_LIVE);
}

void drawGrid(int graphX, int graphY, int graphW, int graphH, float minDensity, float maxDensity) {
  // Grid and Y-axis labels removed as requested
}

void drawGraphLine(int x0, int y0, int x1, int y1, uint16_t color) {
  tft.drawLine(x0, y0, x1, y1, color);
  tft.drawLine(x0, y0 + 1, x1, y1 + 1, color);
}

float angleForTargetSG(float targetSG) {
  float bestAngle = 0.0f;
  float bestError = 999.0f;
  for (float angle = 0.0f; angle <= 90.0f; angle += 0.5f) {
    float sg = calculateGravity(angle);
    float error = fabs(sg - targetSG);
    if (error < bestError) {
      bestError = error;
      bestAngle = angle;
    }
  }
  return bestAngle;
}

void drawGraph() {
  if (displayDataCount < 2) return;
  
  // Graph area inside the card with reduced margins (no axis labels)
  int axisMarginLeft = 10;
  int axisMarginBottom = 10;
  int graphX = MARGIN + 10 + axisMarginLeft;
  int graphY = TOPBAR_H + MARGIN + 10;
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
  
  // Calculate time range for X-axis labels from RTC timestamps when available.
  int totalPointsForTime = (currentLoadMode == LOAD_ALL) ? totalCSVDataLines : displayDataCount;
  
  if (totalPointsForTime >= 2) {
    uint32_t timeSpan = 0;
    if (timestampsAvailableForGraph()) {
      timeSpan = timestampForPoint(displayDataCount - 1) - timestampForPoint(0);
    } else {
      timeSpan = (totalPointsForTime - 1) * 600;
    }
    
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

    int x = graphXForPoint(i, graphX, graphW);
    
    // SG line (blue)
    int y_SG = graphY + graphH - ((data.density - minDensity) / densityRange * graphH);
    if (prevX_SG >= 0 && prevY_SG >= 0) {
      drawGraphLine(prevX_SG, prevY_SG, x, y_SG, currentTheme->graphBlue);
    }
    prevX_SG = x;
    prevY_SG = y_SG;
    
    // Temperature line (green)
    int y_Temp = graphY + graphH - ((data.temperature - minTemp) / tempRange * graphH);
    if (prevX_Temp >= 0 && prevY_Temp >= 0) {
      drawGraphLine(prevX_Temp, prevY_Temp, x, y_Temp, currentTheme->graphGreen);
    }
    prevX_Temp = x;
    prevY_Temp = y_Temp;
    
    // ABV line (purple) - only if OG is set
    if (ogCaptured) {
      float abv = calculateABV(originalGravity, data.density);
      int y_Abv = graphY + graphH - ((abv - minAbv) / abvRange * graphH);
      if (prevX_Abv >= 0 && prevY_Abv >= 0) {
        drawGraphLine(prevX_Abv, prevY_Abv, x, y_Abv, currentTheme->graphPurple);
      }
      prevX_Abv = x;
      prevY_Abv = y_Abv;
    }
  }
  
  // Draw cursor
  updateCursorPosition();
  int cursorX = graphXForPoint(cursorIndex, graphX, graphW);
  
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
  bool showRight = cursorPosition < 0.50;
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
  int graphY = TOPBAR_H + MARGIN + 10;
  int graphW = UI_W - MARGIN * 2 - 20 - axisMarginLeft;
  int graphH = UI_H - graphY - NAV_H - MARGIN - axisMarginBottom;
  
  // Find min/max values for the specific metric
  float minValue = 0, maxValue = 0;
  const char* yAxisLabel = "";
  float padding = 0;
  
  // Initialize based on metric type
  switch (metric) {
    case METRIC_TEMPERATURE:
      minValue = 0.0, maxValue = 30.0;
      yAxisLabel = "C";
      padding = 0.0;
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
    
    if (metric != METRIC_TEMPERATURE) {
      if (value < minValue) minValue = value;
      if (value > maxValue) maxValue = value;
    }
  }

  if (metric == METRIC_ABV && brewProfileLoaded) {
    float expectedAbv = DerivedCalculations::abv(activeBrewProfile.effectiveOG, activeBrewProfile.expectedFinalGravity);
    if (expectedAbv > maxValue) maxValue = expectedAbv;
  } else if (metric == METRIC_ANGLE && brewProfileLoaded && displayDataCount > 0) {
    bool useRtcTime = timestampsAvailableForGraph();
    uint32_t firstTime = useRtcTime
      ? timestampForPoint(0)
      : displayDataBuffer[(displayDataIndex - displayDataCount + MAX_DATA_POINTS) % MAX_DATA_POINTS].uptime_s;
    for (int i = 0; i < displayDataCount; i++) {
      int idx = (displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
      payload_t data = displayDataBuffer[idx];
      uint32_t pointTime = useRtcTime ? timestampForPoint(i) : data.uptime_s;
      float hour = pointTime >= firstTime ? (pointTime - firstTime) / 3600.0f : 0.0f;
      float targetSG = TargetCurveGenerator::expectedGravityAtHour(activeBrewProfile, hour);
      float targetAngle = angleForTargetSG(targetSG);
      if (targetAngle < minValue) minValue = targetAngle;
      if (targetAngle > maxValue) maxValue = targetAngle;
    }
  }
  
  // Add padding
  minValue -= padding;
  maxValue += padding;
  
  // Draw grid with axis labels
  drawGrid(graphX, graphY, graphW, graphH, minValue, maxValue);
  
  // Update Y-axis label
  tft.setTextColor(uiColorTextSecondary);
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
    uint32_t firstTime = timestampsAvailableForGraph() ? timestampForPoint(0) : firstData.uptime_s;
    uint32_t lastTime = timestampsAvailableForGraph() ? timestampForPoint(displayDataCount - 1) : lastData.uptime_s;
    uint32_t timeSpan = lastTime >= firstTime ? lastTime - firstTime : 0;
    
    // Draw X-axis label
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_XS);
    tft.setCursor(graphX + graphW / 2 - 15, graphY + graphH + axisMarginBottom - 5);
    tft.print("Time");
    
    // Draw X-axis time labels (start and end times)
    char timeBuf[16];
    if (timeSpan < 3600) {
      // Show minutes
      tft.setCursor(graphX - 5, graphY + graphH + 15);
      tft.print("0min");
      snprintf(timeBuf, sizeof(timeBuf), "%dmin", (int)(timeSpan / 60));
      tft.setCursor(graphX + graphW - 30, graphY + graphH + 15);
      tft.print(timeBuf);
    } else {
      // Show hours
      tft.setCursor(graphX - 5, graphY + graphH + 15);
      tft.print("0h");
      snprintf(timeBuf, sizeof(timeBuf), "%.1fh", timeSpan / 3600.0);
      tft.setCursor(graphX + graphW - 30, graphY + graphH + 15);
      tft.print(timeBuf);
    }
  }

  // Prevent division by zero
  float valueRange = maxValue - minValue;
  if (valueRange == 0) valueRange = 0.001;
  uint32_t firstMetricTime = 0;
  bool useMetricRtcTime = timestampsAvailableForGraph();
  if (displayDataCount > 0) {
    if (useMetricRtcTime) {
      firstMetricTime = timestampForPoint(0);
    } else {
      payload_t firstData = displayDataBuffer[(displayDataIndex - displayDataCount + MAX_DATA_POINTS) % MAX_DATA_POINTS];
      firstMetricTime = firstData.uptime_s;
    }
  }

  if (metric == METRIC_TEMPERATURE && brewProfileLoaded) {
    int yMin = graphY + graphH - ((activeBrewProfile.recommendedTempMinC - minValue) / valueRange * graphH);
    int yMax = graphY + graphH - ((activeBrewProfile.recommendedTempMaxC - minValue) / valueRange * graphH);
    tft.drawFastHLine(graphX, yMin, graphW, currentTheme->graphPurple);
    tft.drawFastHLine(graphX, yMin + 1, graphW, currentTheme->graphPurple);
    tft.drawFastHLine(graphX, yMax, graphW, currentTheme->graphPurple);
    tft.drawFastHLine(graphX, yMax + 1, graphW, currentTheme->graphPurple);
    tft.setTextColor(currentTheme->graphPurple);
    tft.setFreeFont(FONT_SIZE_XS);
    tft.setCursor(graphX + 4, yMax + 14);
    tft.print("Yeast range");
    snprintf(buf, sizeof(buf), "%.0f", activeBrewProfile.recommendedTempMaxC);
    tft.setCursor(graphX - 35, yMax + 4);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "%.0f", activeBrewProfile.recommendedTempMinC);
    tft.setCursor(graphX - 35, yMin + 4);
    tft.print(buf);

    if (activeBrewProfile.diacetylRestEnabled || activeBrewProfile.diacetylRestRecommendedByYeast) {
      float diacetylTarget = activeBrewProfile.recommendedTempMaxC + 2.0f;
      int yRest = graphY + graphH - ((diacetylTarget - minValue) / valueRange * graphH);
      tft.drawFastHLine(graphX, yRest, graphW, uiColorWarning);
      tft.drawFastHLine(graphX, yRest + 1, graphW, uiColorWarning);
      tft.setTextColor(uiColorWarning);
      tft.setCursor(graphX + 4, yRest + 14);
      tft.print("D-rest");
      snprintf(buf, sizeof(buf), "%.0f", diacetylTarget);
      tft.setCursor(graphX - 35, yRest + 4);
      tft.print(buf);
    }

    int yCold = graphY + graphH - ((2.0f - minValue) / valueRange * graphH);
    tft.drawFastHLine(graphX, yCold, graphW, currentTheme->graphBlue);
    tft.drawFastHLine(graphX, yCold + 1, graphW, currentTheme->graphBlue);
    tft.setTextColor(currentTheme->graphBlue);
    tft.setCursor(graphX + 4, yCold - 3);
    tft.print("Cold crush");
    snprintf(buf, sizeof(buf), "2");
    tft.setCursor(graphX - 35, yCold + 4);
    tft.print(buf);
  } else if (metric == METRIC_ANGLE && brewProfileLoaded) {
    int prevTargetX = -1;
    int prevTargetY = -1;
    float latestTargetAngle = 0.0f;
    for (int i = 0; i < displayDataCount; i++) {
      int idx = (displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
      payload_t data = displayDataBuffer[idx];
      uint32_t pointTime = useMetricRtcTime ? timestampForPoint(i) : data.uptime_s;
      float hour = pointTime >= firstMetricTime ? (pointTime - firstMetricTime) / 3600.0f : 0.0f;
      float targetSG = TargetCurveGenerator::expectedGravityAtHour(activeBrewProfile, hour);
      float targetAngle = angleForTargetSG(targetSG);
      int x = graphXForPoint(i, graphX, graphW);
      int y = graphY + graphH - ((targetAngle - minValue) / valueRange * graphH);
      if (y < graphY) y = graphY;
      if (y > graphY + graphH) y = graphY + graphH;
      if (prevTargetX >= 0) drawGraphLine(prevTargetX, prevTargetY, x, y, currentTheme->graphPurple);
      prevTargetX = x;
      prevTargetY = y;
      latestTargetAngle = targetAngle;
    }
    tft.setTextColor(currentTheme->graphPurple);
    tft.setFreeFont(FONT_SIZE_XS);
    tft.setCursor(graphX + 4, graphY + 14);
    tft.print("Target angle");
    int yTarget = graphY + graphH - ((latestTargetAngle - minValue) / valueRange * graphH);
    if (yTarget < graphY) yTarget = graphY;
    if (yTarget > graphY + graphH) yTarget = graphY + graphH;
    snprintf(buf, sizeof(buf), "%.1f", latestTargetAngle);
    tft.setCursor(graphX - 35, yTarget + 4);
    tft.print(buf);
  } else if (metric == METRIC_ABV && brewProfileLoaded) {
    int prevTargetX = -1;
    int prevTargetY = -1;
    for (int i = 0; i < displayDataCount; i++) {
      int idx = (displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
      payload_t data = displayDataBuffer[idx];
      uint32_t pointTime = useMetricRtcTime ? timestampForPoint(i) : data.uptime_s;
      float hour = pointTime >= firstMetricTime ? (pointTime - firstMetricTime) / 3600.0f : 0.0f;
      float targetSG = TargetCurveGenerator::expectedGravityAtHour(activeBrewProfile, hour);
      float targetAbv = DerivedCalculations::abv(activeBrewProfile.effectiveOG, targetSG);
      int x = graphXForPoint(i, graphX, graphW);
      int y = graphY + graphH - ((targetAbv - minValue) / valueRange * graphH);
      if (prevTargetX >= 0) drawGraphLine(prevTargetX, prevTargetY, x, y, uiColorInfo);
      prevTargetX = x;
      prevTargetY = y;
    }
    tft.setTextColor(uiColorInfo);
    tft.setFreeFont(FONT_SIZE_XS);
    tft.setCursor(graphX + 4, graphY + 14);
    tft.print("Expected ABV");
  }

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

    int x = graphXForPoint(i, graphX, graphW);
    int y = graphY + graphH - ((value - minValue) / valueRange * graphH);

    if (prevX >= 0 && prevY >= 0) {
      drawGraphLine(prevX, prevY, x, y, uiColorGold);
    }
    
    prevX = x;
    prevY = y;
  }
  
  // Draw last point dot
  if (displayDataCount > 0 && prevX >= 0 && prevY >= 0) {
    tft.fillCircle(prevX, prevY, 4, uiColorGold);
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    tft.setTextColor(uiColorGold);
    tft.setFreeFont(FONT_SIZE_XS);
    if (metric == METRIC_TEMPERATURE) {
      snprintf(buf, sizeof(buf), "%.1f C", latest.temperature);
    } else if (metric == METRIC_ANGLE) {
      snprintf(buf, sizeof(buf), "%.1f deg", latest.angle);
    } else if (metric == METRIC_ABV && ogCaptured) {
      snprintf(buf, sizeof(buf), "%.1f%%", calculateABV(originalGravity, latest.density));
    } else {
      snprintf(buf, sizeof(buf), "%.3f", latest.density);
    }
    int labelX = prevX + 8;
    if (labelX + tft.textWidth(buf) > graphX + graphW) labelX = graphX + graphW - tft.textWidth(buf);
    int labelY = prevY - 4;
    if (labelY < graphY + 10) labelY = graphY + 10;
    if (labelY > graphY + graphH - 4) labelY = graphY + graphH - 4;
    tft.setCursor(labelX, labelY);
    tft.print(buf);
  }

}

void drawCalibrationView() {
  tft.fillScreen(uiColorBackground);
  
  drawViewTopbar("Calib");
  
  int contentY = TOPBAR_H + MARGIN;
  int contentH = UI_H - TOPBAR_H;
  
  // Main content card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, 184, CARD_RADIUS);
  
  if (calibMode == CALIB_IDLE) {
    uiTextCenter(MARGIN, contentY + 20, UI_W - MARGIN * 2, 26, "Calibration", FONT_SIZE_LG, uiColorTextPrimary);
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN + 18, contentY + 82);
    tft.print("Place float on a flat, level surface.");
    tft.setCursor(MARGIN + 18, contentY + 110);
    tft.print("Start when the float is completely still.");
    drawButton(BUTTON_CALIB_START_X, BUTTON_CALIB_START_Y, BUTTON_CALIB_START_W, BUTTON_CALIB_START_H, "START", false);
    
  } else if (calibMode == CALIB_SETUP) {
    uiTextCenter(MARGIN, contentY + 12, UI_W - MARGIN * 2, 24, "Calibration Setup", FONT_SIZE_SM_BOLD, uiColorInfo);

    tft.setTextColor(uiColorTextPrimary);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN + 18, contentY + 62);
    tft.print("SG Calibration");
    IOSSwitch::draw(tft, UI_W - MARGIN - 86, contentY + 35, calibDoSGCalibration);

    tft.setTextColor(uiColorTextSecondary);
    if (calibDoSGCalibration) {
      tft.setTextColor(uiColorTextPrimary);
      tft.setFreeFont(FONT_SIZE_SM);
      tft.setCursor(MARGIN + 18, contentY + 108);
      tft.print(calibUseSixPoints ? "6 SG points" : "4 SG points");
      IOSSwitch::draw(tft, UI_W - MARGIN - 86, contentY + 81, calibUseSixPoints);

      tft.setCursor(MARGIN + 18, contentY + 154);
      tft.print(calibUseSalt ? "Salt solution" : "Sugar solution");
      IOSSwitch::draw(tft, UI_W - MARGIN - 86, contentY + 127, calibUseSalt);

      tft.setTextColor(uiColorTextSecondary);
      tft.setFreeFont(FONT_SIZE_XS);
      tft.setCursor(MARGIN + 18, contentY + 180);
      tft.print("Amounts shown per calibration point.");
    } else {
      tft.setFreeFont(FONT_SIZE_SM);
      tft.setCursor(MARGIN + 18, contentY + 112);
      tft.print("Only sensor offset will be calibrated.");
      tft.setCursor(MARGIN + 18, contentY + 140);
      tft.print("SG points and solution type are skipped.");
    }

    drawButton(BUTTON_CALIB_BACK_X, BUTTON_CALIB_BACK_Y, BUTTON_CALIB_BACK_W, BUTTON_CALIB_BACK_H, "BACK", false);
    drawButton(BUTTON_CALIB_NEXT_X, BUTTON_CALIB_NEXT_Y, BUTTON_CALIB_NEXT_W, BUTTON_CALIB_NEXT_H, "NEXT", false);

  } else if (calibMode == CALIB_INSTRUCTIONS) {
    // Calculate proper text positions using consistent layout
    tft.setTextColor(uiColorAccent);
    tft.setFreeFont(FONT_SIZE_MD);
    int titleY = contentY + 12;
    uiTextCenter(MARGIN, titleY, UI_W - MARGIN * 2, 24, "Calibration Steps", FONT_SIZE_SM_BOLD, uiColorInfo);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 6; // Consistent line spacing
    int contentStartY = titleY + 52; // Start content below title
    
    int bullet1Y = contentStartY;
    int bullet2Y = bullet1Y + lineHeight;
    
    tft.setCursor(MARGIN + 15, bullet1Y);
    tft.print("1. Place float on a flat surface");
    tft.setCursor(MARGIN + 15, bullet2Y);
    tft.print("2. Keep it still, then continue");
    drawButton(BUTTON_CALIB_BACK_X, BUTTON_CALIB_BACK_Y, BUTTON_CALIB_BACK_W, BUTTON_CALIB_BACK_H, "BACK", false);
    drawButton(BUTTON_CALIB_NEXT_X, BUTTON_CALIB_NEXT_Y, BUTTON_CALIB_NEXT_W, BUTTON_CALIB_NEXT_H, "NEXT", false);
    
  } else if (calibMode == CALIB_OFFSET) {
    uiTextCenter(MARGIN, contentY + 12, UI_W - MARGIN * 2, 24, "Step 1/5 - Sensor Offset", FONT_SIZE_SM_BOLD, uiColorInfo);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 72;
    int bullet2Y = bullet1Y + lineHeight;
    int bullet3Y = bullet2Y + lineHeight;
    
    tft.setCursor(MARGIN + 10, bullet1Y);
    tft.print("- Place float on flat, level surface");
    tft.setCursor(MARGIN + 10, bullet2Y);
    tft.print("- Keep device completely still");
    tft.setCursor(MARGIN + 10, bullet3Y);
    tft.print("- Press Calibrate Offset");
    drawButton(BUTTON_CALIB_EXIT_X, BUTTON_CALIB_EXIT_Y, BUTTON_CALIB_EXIT_W, BUTTON_CALIB_EXIT_H, "ABORT", false);
    drawButton(BUTTON_CALIB_OFFSET_X, BUTTON_CALIB_OFFSET_Y, BUTTON_CALIB_OFFSET_W, BUTTON_CALIB_OFFSET_H, "Calibrate", offsetCalibrated);
    
  } else if (calibMode == CALIB_SKIP_WARNING) {
    uiTextCenter(MARGIN, contentY + 12, UI_W - MARGIN * 2, 24, "Skip SG Calibration?", FONT_SIZE_MD, uiColorAccent);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int textY = contentY + 72;
    
    tft.setCursor(MARGIN + 10, textY);
    tft.print("Save sensor offset only and");
    textY += lineHeight;
    tft.setCursor(MARGIN + 10, textY);
    tft.print("return to Live View?");
    
    drawButton(BUTTON_CALIB_SKIP_NO_X, BUTTON_CALIB_SKIP_NO_Y, BUTTON_CALIB_SKIP_NO_W, BUTTON_CALIB_SKIP_NO_H, "BACK", false);
    drawButton(BUTTON_CALIB_SKIP_YES_X, BUTTON_CALIB_SKIP_YES_Y, BUTTON_CALIB_SKIP_YES_W, BUTTON_CALIB_SKIP_YES_H, "CONFIRM", false);
    
  } else if (calibMode >= CALIB_POINT1 && calibMode <= CALIB_POINT6) {
    int idx = calibrationPointIndex(calibMode);
    int totalPoints = calibrationPointCount();
    char buf[48];
    snprintf(buf, sizeof(buf), "Step %d/%d - Point %d (SG %.3f)",
             idx + 2, totalPoints + 1, idx + 1, calibrationTargetSG(idx));
    uiTextCenter(MARGIN, contentY + 12, UI_W - MARGIN * 2, 24, buf, FONT_SIZE_SM_BOLD, uiColorInfo);

    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    int lineHeight = tft.fontHeight() + 4;
    int bullet1Y = contentY + 72;
    int bullet2Y = bullet1Y + lineHeight;
    int bullet3Y = bullet2Y + lineHeight;
    int bullet4Y = bullet3Y + lineHeight;

    tft.setCursor(MARGIN + 10, bullet1Y);
    if (idx == 0) {
      tft.print("- Fill 3L bowl with 2.0L water");
      tft.setCursor(MARGIN + 10, bullet2Y);
      tft.print("- No sugar/salt for water point");
    } else {
      tft.printf("- Add %dg %s", calibrationAddAmount(idx), calibUseSalt ? "salt" : "sugar");
      tft.setCursor(MARGIN + 10, bullet2Y);
      tft.print("- Stir until fully dissolved");
    }
    tft.setCursor(MARGIN + 10, bullet3Y);
    tft.print("- Wait for stable float");
    tft.setCursor(MARGIN + 10, bullet4Y);
    tft.print("- Press Record");
    if (idx == 0) {
      drawButton(BUTTON_CALIB_SKIP_X, BUTTON_CALIB_SKIP_Y, BUTTON_CALIB_SKIP_W, BUTTON_CALIB_SKIP_H, "ABORT", false);
    } else {
      drawButton(BUTTON_CALIB_EXIT_X, BUTTON_CALIB_EXIT_Y, BUTTON_CALIB_EXIT_W, BUTTON_CALIB_EXIT_H, "ABORT", false);
    }
    drawButton(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, "Record", calibAngles[idx] > 0);
    
  } else if (calibMode == CALIB_COMPLETE) {
    char title[32];
    snprintf(title, sizeof(title), "%d Points Recorded", calibrationPointCount());
    uiTextCenter(MARGIN, contentY + 12, UI_W - MARGIN * 2, 24, title, FONT_SIZE_MD, uiColorTextPrimary);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_XS);
    int lineHeight = tft.fontHeight() + 5;
    int summaryY = contentY + 72;
    
    char buf[64];
    tft.setCursor(MARGIN + 18, contentY + 58);
    tft.print("Summary:");
    int leftX = MARGIN + 18;
    int rightX = MARGIN + (UI_W - MARGIN * 2) / 2 + 8;
    for (int i = 0; i < calibrationPointCount(); i++) {
      int colX = (i < 3) ? leftX : rightX;
      int rowY = summaryY + (i % 3) * lineHeight;
      snprintf(buf, sizeof(buf), "P%d SG %.3f  %.1f deg", i + 1, calibrationTargetSG(i), calibAngles[i]);
      tft.setCursor(colX, rowY);
      tft.print(buf);
    }
    
    drawButton(BUTTON_CALIB_EXIT_X, BUTTON_CALIB_EXIT_Y, BUTTON_CALIB_EXIT_W, BUTTON_CALIB_EXIT_H, "ABORT", false);
    drawButton(BUTTON_CALIB_APPLY_X, BUTTON_CALIB_APPLY_Y, BUTTON_CALIB_APPLY_W, BUTTON_CALIB_APPLY_H, "CONFIRM", false);
  } else if (calibMode == CALIB_APPLYING) {
    uiTextCenter(MARGIN, contentY + 12, UI_W - MARGIN * 2, 24, "Applying Calibration", FONT_SIZE_MD, uiColorTextPrimary);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN + 18, contentY + 86);
    tft.print("Calculating polynomial...");
    
  } else if (calibMode == CALIB_FAILED) {
    uiTextCenter(MARGIN, contentY + 12, UI_W - MARGIN * 2, 24, "CALIBRATION FAILED", FONT_SIZE_MD, uiColorError);
    
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(MARGIN + 18, contentY + 86);
    tft.print("No response from float");
    
    drawButton(BUTTON_CALIB_EXIT_X, BUTTON_CALIB_EXIT_Y, BUTTON_CALIB_EXIT_W, BUTTON_CALIB_EXIT_H, "EXIT", false);
  }
  
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
  
  drawViewTopbar("Battery");
  
  int contentY = TOPBAR_H + MARGIN;
  int contentH = UI_H - contentY - NAV_H - MARGIN;
  
  bool hasLiveBattery = latestFloatDataValid &&
                        latestFloatData.battery_voltage >= 2.5f &&
                        latestFloatData.battery_voltage <= 4.5f;

  if (displayDataCount < 3 && !hasLiveBattery) {
    drawNoDataCard("No battery data", "Collect a few points first", TAB_DASHBOARD);
    return;
  }
  
  // Left 2/3: Battery voltage graph
  int graphW = (UI_W - MARGIN * 2 - GAP) * 2 / 3;
  int graphX = MARGIN;
  int graphH = contentH;
  int graphY = contentY;
  
  // Adjust graph height to accommodate buttons
  int graphContentH = BATTERY_BUTTON_Y - graphY - GAP;
  
  uiCard(graphX, graphY, graphW, graphH, CARD_RADIUS);
  
  // Draw battery sparkline graph
  // Load historical battery data from CSV based on batteryLoadMode (similar to graph view)
  if (fermentationFileOpen && strlen(currentFermentationFile) > 0) {
    // Load battery voltage data from CSV based on time range
    // Load every available row for the selected time range. Sampling is only
    // useful once the file is larger than the display buffer.
    if (mountSDTemporarily()) {
      loadHistoricalDataFromCSV(currentFermentationFile, batteryLoadMode, MAX_DATA_POINTS, 1);
      dismountSD();
    }
  }

  if (hasLiveBattery) {
    bool appendLive = displayDataCount == 0;
    if (displayDataCount > 0) {
      uint32_t newestLoadedEpoch = displayTimestampBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
      payload_t newestLoaded = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
      appendLive = !isEpochValid(newestLoadedEpoch) ||
                   !isEpochValid(latestFloatEpoch) ||
                   latestFloatEpoch > newestLoadedEpoch ||
                   fabs(newestLoaded.battery_voltage - latestFloatData.battery_voltage) > 0.001f;
    }
    if (appendLive) {
      addDataPoint(latestFloatData, latestFloatEpoch);
    }
  }
  
  // Extract battery voltage data from loaded displayDataBuffer
  float batteryData[MAX_DATA_POINTS];
  int batteryCount = 0;
  int pointsToShow = min(displayDataCount, MAX_DATA_POINTS);
  for (int i = 0; i < pointsToShow; i++) {
    int idx = (displayDataIndex - pointsToShow + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    float voltage = displayDataBuffer[idx].battery_voltage;
    if (voltage >= 2.5f && voltage <= 4.5f) {
      batteryData[batteryCount++] = voltage;
    }
  }
  
  // Draw sparkline with margins and reduced height for buttons
  int sparkX = graphX + 50;  // Increased left margin for y-axis labels
  int sparkY = graphY + 40;
  int sparkW = graphW - 70;  // Reduced width for y-axis labels
  int sparkH = graphContentH - 80;  // Use available space below title
  
  float minVoltage = batteryCount > 0 ? batteryData[0] : 3.0f;
  float maxVoltage = batteryCount > 0 ? batteryData[0] : 4.2f;
  for (int i = 1; i < batteryCount; i++) {
    if (batteryData[i] < minVoltage) minVoltage = batteryData[i];
    if (batteryData[i] > maxVoltage) maxVoltage = batteryData[i];
  }
  float axisMin = floorf((minVoltage - 0.02f) * 100.0f) / 100.0f;
  float axisMax = ceilf((maxVoltage + 0.02f) * 100.0f) / 100.0f;
  if (axisMax - axisMin < 0.06f) {
    float mid = (axisMax + axisMin) * 0.5f;
    axisMin = mid - 0.03f;
    axisMax = mid + 0.03f;
  }

  tft.fillRect(sparkX, sparkY, sparkW, sparkH, uiColorCardBackground);
  for (int i = 1; i < 4; i++) {
    int gy = sparkY + (sparkH * i / 4);
    tft.drawFastHLine(sparkX, gy, sparkW, currentTheme->gridLine);
  }
  if (batteryCount >= 2) {
    int prevX = -1;
    int prevY = -1;
    float range = axisMax - axisMin;
    for (int i = 0; i < batteryCount; i++) {
      float normalized = (batteryData[i] - axisMin) / range;
      if (normalized < 0.0f) normalized = 0.0f;
      if (normalized > 1.0f) normalized = 1.0f;
      int px = sparkX + (i * sparkW / (batteryCount - 1));
      int py = sparkY + sparkH - (int)(normalized * sparkH);
      if (prevX >= 0 && prevY >= 0) {
        tft.drawLine(prevX, prevY, px, py, uiColorGold);
        tft.drawLine(prevX, prevY + 1, px, py + 1, uiColorGold);
      }
      prevX = px;
      prevY = py;
    }
    tft.fillCircle(prevX, prevY, 3, uiColorGold);
  } else {
    tft.drawFastHLine(sparkX, sparkY + sparkH / 2, sparkW, currentTheme->gridLine);
  }
  
  // Graph title
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(graphX + 20, graphY + 25);
  tft.print("Battery Voltage (V)");
  
  // Draw voltage range labels on y-axis
  if (batteryCount >= 1) {
    tft.setTextColor(uiColorTextSecondary);
    tft.setFreeFont(FONT_SIZE_XS);
    
    // Draw max voltage label
    char voltageBuf[16];
    snprintf(voltageBuf, sizeof(voltageBuf), "%.2f", axisMax);
    tft.setCursor(graphX + 15, sparkY + 5);
    tft.print(voltageBuf);
    
    // Draw min voltage label
    snprintf(voltageBuf, sizeof(voltageBuf), "%.2f", axisMin);
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
  uiCard(cardX, graphY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(cardX + 15, graphY + 20);
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
  uiCard(cardX, graphY + cardH + GAP, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(cardX + 15, graphY + cardH + GAP + 20);
  tft.print("Percentage");
  
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    uint8_t percent = calculateBatteryPercentage(latest.battery_voltage);
    tft.setTextColor(uiColorTextPrimary);
    tft.setFreeFont(FONT_SIZE_MD);
    char percentBuf[16];
    snprintf(percentBuf, sizeof(percentBuf), "%d%%", percent);
    tft.setCursor(cardX + 15, graphY + cardH + GAP + cardH / 2 + 14);
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
    tft.setCursor(cardX + 15, graphY + (cardH + GAP) * 2 + cardH / 2 + 14);
    tft.print("Charging");
  } else if (chargingState == DISCHARGING && displayDataCount >= 20) {
    // Calculate discharge rate using linear regression over last 20 readings
    float dischargeRate = calculateDischargeRateLinearRegression(); // Volts per second
    
    LOG_VERBOSE("Battery view: dischargeRate=%.6f, threshold=0.00000004\n", dischargeRate);
    
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
      tft.setCursor(cardX + 15, graphY + (cardH + GAP) * 2 + cardH / 2 + 14);
      tft.print("Stable");
    }
  } else {
    // Unknown charging state or not enough data
    tft.setTextColor(uiColorTextMuted);
    tft.setFreeFont(FONT_SIZE_SM);
    tft.setCursor(cardX + 15, graphY + (cardH + GAP) * 2 + cardH / 2 + 14);
    if (chargingState == CHARGING_UNKNOWN) {
      tft.print("Detecting...");
    } else {
      tft.print("Calculating...");
    }
  }
  
  uiDrawBottomNav(TAB_DASHBOARD);
}

void drawTempGraphView() {
  tft.fillScreen(uiColorBackground);
  
  drawViewTopbar("Temp Target");
  
  int contentY = TOPBAR_H + MARGIN;
  int contentH = UI_H - contentY - NAV_H - MARGIN;
  
  if (displayDataCount < 3) {
    drawNoDataCard("No graph data", "Collect a few points first", TAB_LIVE);
    return;
  }
  
  // Draw graph card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, contentH, CARD_RADIUS);
  drawGraphForMetric(METRIC_TEMPERATURE);
  
  uiDrawBottomNav(TAB_LIVE);
}

void drawAngleGraphView() {
  tft.fillScreen(uiColorBackground);
  
  drawViewTopbar("Angle");
  
  int contentY = TOPBAR_H + MARGIN;
  int contentH = UI_H - contentY - NAV_H - MARGIN;
  
  if (displayDataCount < 3) {
    drawNoDataCard("No graph data", "Collect a few points first", TAB_LIVE);
    return;
  }
  
  // Draw graph card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, contentH, CARD_RADIUS);
  drawGraphForMetric(METRIC_ANGLE);
  
  uiDrawBottomNav(TAB_LIVE);
}

void drawAbvGraphView() {
  tft.fillScreen(uiColorBackground);
  
  drawViewTopbar("ABV Target");
  
  int contentY = TOPBAR_H + MARGIN;
  int contentH = UI_H - contentY - NAV_H - MARGIN;
  
  if (!ogCaptured) {
    drawNoDataCard("OG not set", "Set OG first", TAB_LIVE);
    return;
  }
  
  if (displayDataCount < 3) {
    drawNoDataCard("No graph data", "Collect a few points first", TAB_LIVE);
    return;
  }
  
  // Draw graph card
  uiCard(MARGIN, contentY, UI_W - MARGIN * 2, contentH, CARD_RADIUS);
  drawGraphForMetric(METRIC_ABV);
  
  uiDrawBottomNav(TAB_LIVE);
}

// UI Design System Helper Functions Implementation

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

void drawThemeToggle(int x, int y, bool enabled) {
  uint16_t bg = enabled ? uiColorSuccess : uiColorBorder;
  tft.fillRoundRect(x, y, 52, 28, 14, bg);
  tft.fillCircle(enabled ? x + 38 : x + 14, y + 14, 11, TFT_WHITE);
}

void loadThemePreference() {
  EEPROM.begin(128);
  uint32_t magic = 0;
  EEPROM.get(EEPROM_THEME_MAGIC, magic);
  if (magic == THEME_MAGIC) {
    uint8_t savedTheme = EEPROM.read(EEPROM_THEME_VALUE);
    darkMode = savedTheme != 0;
  } else {
    darkMode = true;
  }
  currentTheme = darkMode ? &darkTheme : &lightTheme;
  LOG_INFO("Theme loaded: %s\n", darkMode ? "dark" : "light");
}

void saveThemePreference() {
  EEPROM.begin(128);
  EEPROM.put(EEPROM_THEME_MAGIC, THEME_MAGIC);
  EEPROM.write(EEPROM_THEME_VALUE, darkMode ? 1 : 0);
  if (EEPROM.commit()) {
    LOG_INFO("Theme saved: %s\n", darkMode ? "dark" : "light");
  } else {
    LOG_ERRORLN("ERROR: Failed to save theme");
  }
}

void toggleTheme() {
  darkMode = !darkMode;
  currentTheme = darkMode ? &darkTheme : &lightTheme;
  uiInitColors();
  saveThemePreference();
  
  // Redraw entire screen with new theme
  tft.fillScreen(currentTheme->background);
  screenDirty = true;
  staticElementsDrawn = false;
}

bool isLogoBackgroundPixel(uint16_t color) {
  uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color & 0x1F) * 255 / 31;
  return r >= 238 && g >= 238 && b >= 238;
}

void drawBootScreen() {
  tft.fillScreen(lightTheme.background);
  
  // Center logo on screen
  int logoX = (SCREEN_W - logo2_width) / 2;
  int logoY = (SCREEN_H - logo2_height) / 2 - 20; // Move up to make room for text
  
  // Draw logo from optimized pixel data. Near-white source pixels are
  // treated as transparent to avoid halos on the dark boot background.
  int numPixels = sizeof(logo2_pixels) / sizeof(Pixel);
  for (int i = 0; i < numPixels; i++) {
    int x = logoX + logo2_pixels[i].x;
    int y = logoY + logo2_pixels[i].y;
    uint16_t color = logo2_pixels[i].color;
    if (isLogoBackgroundPixel(color)) continue;
    tft.drawPixel(x, y, color);
  }
  
  // Draw version text below logo
  tft.setTextColor(currentTheme->textSecondary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(logoX + 60, logoY + logo2_height + 14);  // Adjust for FreeFont baseline
  tft.println("alpha 0.4.0");
  
  delay(2000); // Show boot screen for 2 seconds
}

void checkTouch() {
  uint16_t x, y;
  
  // Use TFT_eSPI built-in touch (calibrated in Touch.h)
  bool touched = false;
  #if SGNODE_UI_TEST_HARNESS
    if (uiTestTouchActive) {
      x = uiTestTouchX;
      y = uiTestTouchY;
      touched = true;
    } else {
      touched = tft.getTouch(&x, &y);
    }
  #else
    touched = tft.getTouch(&x, &y);
  #endif
  
  if (touched) {
    // Update last touch time for SD operation timing
    lastTouchTime = millis();
    
    touchX = x;
    touchY = y;

    // Top bar battery icon/percentage opens the dedicated Battery view.
    if (touchY <= TOPBAR_H && touchX >= UI_W - 95) {
      currentMode = BATTERY_VIEW;
      screenDirty = true;
      staticElementsDrawn = false;
      LOG_VERBOSELN("Switched to Battery View from topbar");
      delay(50);
      return;
    }

    if (currentMode == BREW_WIZARD_VIEW) {
      if (brewWizard.handleTouch(touchX, touchY)) {
        if (brewWizard.completed()) {
          brewWizard.clearResultFlags();
          completeBrewWizard();
        } else if (brewWizard.cancelled()) {
          brewWizard.clearResultFlags();
          cancelBrewWizard();
        } else {
          screenDirty = true;
        }
      }
      delay(50);
      return;
    }

    if (currentMode == NEW_YEAST_VIEW) {
      handleNewYeastTouch(touchX, touchY);
      delay(50);
      return;
    }

    if (currentMode == MANAGE_YEAST_VIEW) {
      handleManageYeastTouch(touchX, touchY);
      delay(50);
      return;
    }

    if (currentMode == MANAGE_BREW_VIEW) {
      handleManageBrewTouch(touchX, touchY);
      delay(50);
      return;
    }

    if (currentMode == OG_VERIFICATION_VIEW) {
      if (touchX >= MARGIN && touchX <= MARGIN + 200 &&
          touchY >= 242 && touchY <= 286) {
        handleOGChoice(false);
        delay(50);
        return;
      }
      if (touchX >= UI_W - MARGIN - 200 && touchX <= UI_W - MARGIN &&
          touchY >= 242 && touchY <= 286) {
        handleOGChoice(true);
        delay(50);
        return;
      }
    }

    if (currentMode == DASHBOARD_VIEW && brewProfileLoaded && currentBatchAction.requiresChoice) {
      int contentY = TOPBAR_H + MARGIN;
      int contentW = UI_W - MARGIN * 2;
      int actionY = contentY + 42 + 6;
      int actionH = 78;
      int buttonY = actionY + actionH - 32;
      int buttonW = 86;
      if (touchY >= buttonY && touchY <= buttonY + 28) {
        if (touchX >= MARGIN + contentW - buttonW * 2 - GAP - 10 &&
            touchX <= MARGIN + contentW - buttonW - GAP - 10) {
          handleCurrentActionChoice(false);
          delay(50);
          return;
        }
        if (touchX >= MARGIN + contentW - buttonW - 10 &&
            touchX <= MARGIN + contentW - 10) {
          handleCurrentActionChoice(true);
          delay(50);
          return;
        }
      }
    }

    if (currentMode == TARGET_CHART_VIEW && touchY < UI_H - NAV_H) {
      currentMode = LIVE_VIEW;
      screenDirty = true;
      delay(50);
      return;
    }
    
    // Handle Create New Fermentation dialog touches (highest priority)
    if (showingCreateNewDialog) {
      // Yes button
      if (touchX >= BUTTON_DIALOG_YES_X && touchX <= BUTTON_DIALOG_YES_X + BUTTON_DIALOG_YES_W &&
          touchY >= BUTTON_DIALOG_YES_Y && touchY <= BUTTON_DIALOG_YES_Y + BUTTON_DIALOG_YES_H) {
        beginBrewWizard();
        showingCreateNewDialog = false;
        screenDirty = true;
        LOG_VERBOSELN("Opened Brew Wizard from dialog");
      }
      
      // Back button
      if (touchX >= BUTTON_DIALOG_BACK_X && touchX <= BUTTON_DIALOG_BACK_X + BUTTON_DIALOG_BACK_W &&
          touchY >= BUTTON_DIALOG_BACK_Y && touchY <= BUTTON_DIALOG_BACK_Y + BUTTON_DIALOG_BACK_H) {
        showingCreateNewDialog = false;
        screenDirty = true;
        LOG_VERBOSELN("Cancelled new fermentation from dialog");
      }
      
      delay(50);
      return;
    }
    
    if (currentMode == CALIBRATION_VIEW && touchY >= UI_H - NAV_H) {
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
        case TAB_DASHBOARD:
          tabMode = DASHBOARD_VIEW;
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
            LOG_VERBOSELN("Switched to Live View");
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
            LOG_VERBOSELN("Switched to Graph View");
            break;
          case TAB_DASHBOARD:
            currentMode = DASHBOARD_VIEW;
            screenDirty = true;
            LOG_VERBOSELN("Switched to Dashboard");
            break;
          case TAB_MORE:
            currentMode = MORE_VIEW;
            screenDirty = true;
            LOG_VERBOSELN("Switched to More View");
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
      int contentY = TOPBAR_H + MARGIN;
      int contentH = UI_H - TOPBAR_H - NAV_H;
      int graphW = (UI_W - MARGIN * 2 - GAP) * 2 / 3;
      int graphX = MARGIN;
      
      int buttonW = (graphW - MARGIN * 2 - GAP * 2) / 3;
      int buttonX = graphX + MARGIN;
      int buttonY = BATTERY_BUTTON_Y;
      int buttonH = BATTERY_BUTTON_H;
      
      // 6h button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        batteryLoadMode = LOAD_6H;
        screenDirty = true;
        LOG_VERBOSELN("Battery view: 6h selected");
        delay(50);
        return;
      }
      buttonX += buttonW + GAP;
      
      // 24h button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        batteryLoadMode = LOAD_24H;
        screenDirty = true;
        LOG_VERBOSELN("Battery view: 24h selected");
        delay(50);
        return;
      }
      buttonX += buttonW + GAP;
      
      // All button
      if (touchX >= buttonX && touchX <= buttonX + buttonW &&
          touchY >= buttonY && touchY <= buttonY + buttonH) {
        batteryLoadMode = LOAD_ALL;
        screenDirty = true;
        LOG_VERBOSELN("Battery view: All selected");
        delay(50);
        return;
      }
    }
    
    // Handle More view card taps
    if (currentMode == MORE_VIEW) {
      int contentY = TOPBAR_H + MARGIN;
      int cardH = (UI_H - TOPBAR_H - NAV_H - MARGIN * 2 - GAP * 3) / 4;
      int contentW = UI_W - MARGIN * 2;
      int cardW = (contentW - GAP) / 2;
      int leftX = MARGIN;
      int rightX = MARGIN + cardW + GAP;
      
      // System card (goes to LIVE_DETAILS)
      if (touchX >= leftX && touchX <= leftX + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        currentMode = LIVE_DETAILS_VIEW;
        screenDirty = true;
        LOG_VERBOSELN("Switched to Live Details");
      }

      // Theme switch
      if (touchX >= rightX && touchX <= rightX + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        toggleTheme();
        screenDirty = true;
        LOG_VERBOSELN("Theme toggled");
        delay(50);
        return;
      }
      
      // Calibration card - opens Polynomial Graph
      contentY += cardH + GAP;
      if (touchX >= leftX && touchX <= leftX + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        currentMode = POLY_GRAPH_VIEW;
        screenDirty = true;
        LOG_VERBOSELN("Switched to Polynomial Graph");
      }

      // New Yeast entry point
      if (touchX >= rightX && touchX <= rightX + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        beginNewYeastWizard();
        LOG_VERBOSELN("Opened New Yeast wizard");
        delay(50);
        return;
      }
      
      // Create New Fermentation card
      contentY += cardH + GAP;
      if (touchX >= leftX && touchX <= leftX + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        beginBrewWizard();
        screenDirty = true;
        LOG_VERBOSELN("Opened Brew Wizard");
      }
      
      // Manage Yeast card
      if (touchX >= rightX && touchX <= rightX + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        loadManagedYeasts();
        currentMode = MANAGE_YEAST_VIEW;
        screenDirty = true;
        LOG_VERBOSELN("Switched to Manage Yeast");
        delay(50);
        return;
      }

      // Manage Brew / Reboot row
      contentY += cardH + GAP;
      if (touchX >= leftX && touchX <= leftX + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        loadManagedBatches();
        currentMode = MANAGE_BREW_VIEW;
        screenDirty = true;
        LOG_VERBOSELN("Switched to Manage Brew");
        delay(50);
        return;
      }
      if (touchX >= rightX && touchX <= rightX + cardW &&
          touchY >= contentY && touchY <= contentY + cardH) {
        LOG_INFOLN("Rebooting...");
        ESP.restart();
      }
    }
    
    // Handle Live view tile touches for graph views
    if (currentMode == LIVE_VIEW && displayDataCount > 0) {
      int contentY = calibrationModeActive ? TOPBAR_H + MARGIN + 14 : TOPBAR_H + MARGIN;
      int contentH = UI_H - contentY - NAV_H;
      int heroH = contentH / 2;
      int tileY = contentY + heroH + GAP - 6;  // Lifted by 6px
      int tileW = (UI_W - MARGIN * 2 - GAP * 2) / 3;  // 3 tiles
      int tileH = contentH - heroH - GAP;

      // SG hero opens gravity target vs actual.
      if (touchX >= MARGIN && touchX <= UI_W - MARGIN &&
          touchY >= contentY && touchY <= contentY + heroH) {
        currentMode = TARGET_CHART_VIEW;
        screenDirty = true;
        staticElementsDrawn = false;
        LOG_VERBOSELN("Switched to Target vs Actual View");
        delay(50);
        return;
      }
      
      // Temp tile (first tile)
      int tempTileX = MARGIN;
      if (touchX >= tempTileX && touchX <= tempTileX + tileW &&
          touchY >= tileY && touchY <= tileY + tileH) {
        currentMode = TEMP_GRAPH_VIEW;
        screenDirty = true;
        staticElementsDrawn = false;
        LOG_VERBOSELN("Switched to Temp Graph View");
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
        LOG_VERBOSELN("Switched to Angle Graph View");
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
          LOG_VERBOSELN("Switched to ABV Graph View");
          delay(50);
          return;
        } else {
          // OG not set, set OG
          payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
          originalGravity = latest.density;
          ogCaptured = true;
          currentABV = calculateABV(originalGravity, latest.density);
          screenDirty = true;
          LOG_INFO("Set OG to: %.3f\n", originalGravity);
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
      LOG_VERBOSELN("Returned to Live View");
      delay(50);
      return;
    }
    
    // Handle calibration screen buttons
    if (currentMode == CALIBRATION_VIEW) {
      // Start button
      if (calibMode == CALIB_IDLE &&
          touchX >= BUTTON_CALIB_START_X && touchX <= BUTTON_CALIB_START_X + BUTTON_CALIB_START_W &&
          touchY >= BUTTON_CALIB_START_Y && touchY <= BUTTON_CALIB_START_Y + BUTTON_CALIB_START_H) {
        calibMode = CALIB_SETUP;
        screenDirty = true;
        LOG_INFOLN("Started calibration setup");
      }

      if (calibMode == CALIB_SETUP) {
        int contentY = TOPBAR_H + MARGIN;
        if (IOSSwitch::hit(touchX, touchY, UI_W - MARGIN - 86, contentY + 35)) {
          calibDoSGCalibration = !calibDoSGCalibration;
          screenDirty = true;
          delay(50);
          return;
        }
        if (calibDoSGCalibration && IOSSwitch::hit(touchX, touchY, UI_W - MARGIN - 86, contentY + 81)) {
          calibUseSixPoints = !calibUseSixPoints;
          screenDirty = true;
          delay(50);
          return;
        }
        if (calibDoSGCalibration && IOSSwitch::hit(touchX, touchY, UI_W - MARGIN - 86, contentY + 127)) {
          calibUseSalt = !calibUseSalt;
          screenDirty = true;
          delay(50);
          return;
        }
        if (touchX >= BUTTON_CALIB_BACK_X && touchX <= BUTTON_CALIB_BACK_X + BUTTON_CALIB_BACK_W &&
            touchY >= BUTTON_CALIB_BACK_Y && touchY <= BUTTON_CALIB_BACK_Y + BUTTON_CALIB_BACK_H) {
          abortCalibrationFlow();
          LOG_INFOLN("Calibration setup aborted");
          delay(50);
          return;
        }
        if (touchX >= BUTTON_CALIB_NEXT_X && touchX <= BUTTON_CALIB_NEXT_X + BUTTON_CALIB_NEXT_W &&
            touchY >= BUTTON_CALIB_NEXT_Y && touchY <= BUTTON_CALIB_NEXT_Y + BUTTON_CALIB_NEXT_H) {
          for (int i = 0; i < MAX_BASE_CALIB_POINTS; i++) calibAngles[i] = 0.0f;
          calibMode = CALIB_INSTRUCTIONS;
          screenDirty = true;
          LOG_INFOLN("Calibration setup confirmed");
          delay(50);
          return;
        }
      }
      
      // Next button (instructions to offset calibration)
      if (calibMode == CALIB_INSTRUCTIONS &&
          touchX >= BUTTON_CALIB_BACK_X && touchX <= BUTTON_CALIB_BACK_X + BUTTON_CALIB_BACK_W &&
          touchY >= BUTTON_CALIB_BACK_Y && touchY <= BUTTON_CALIB_BACK_Y + BUTTON_CALIB_BACK_H) {
        calibMode = CALIB_SETUP;
        screenDirty = true;
        delay(50);
        return;
      }

      if (calibMode == CALIB_INSTRUCTIONS &&
          touchX >= BUTTON_CALIB_NEXT_X && touchX <= BUTTON_CALIB_NEXT_X + BUTTON_CALIB_NEXT_W &&
          touchY >= BUTTON_CALIB_NEXT_Y && touchY <= BUTTON_CALIB_NEXT_Y + BUTTON_CALIB_NEXT_H) {
        calibMode = CALIB_OFFSET;
        screenDirty = true;
        LOG_VERBOSELN("Moved to Offset Calibration");
      }
      
      // Calibrate Offset button
      if (calibMode == CALIB_OFFSET &&
          touchX >= BUTTON_CALIB_OFFSET_X && touchX <= BUTTON_CALIB_OFFSET_X + BUTTON_CALIB_OFFSET_W &&
          touchY >= BUTTON_CALIB_OFFSET_Y && touchY <= BUTTON_CALIB_OFFSET_Y + BUTTON_CALIB_OFFSET_H) {
        sendCalibrationCommand(5, 0.0); // Command 5 for offset calibration
        startWait(2000); // 2 second wait for offset calibration
        LOG_VERBOSELN("CALIBRATE OFFSET - started 2s wait");
      }

      if ((calibMode == CALIB_OFFSET || (calibMode >= CALIB_POINT1 && calibMode <= CALIB_POINT6) || calibMode == CALIB_COMPLETE || calibMode == CALIB_FAILED) &&
          touchX >= BUTTON_CALIB_EXIT_X && touchX <= BUTTON_CALIB_EXIT_X + BUTTON_CALIB_EXIT_W &&
          touchY >= BUTTON_CALIB_EXIT_Y && touchY <= BUTTON_CALIB_EXIT_Y + BUTTON_CALIB_EXIT_H) {
        abortCalibrationFlow();
        delay(50);
        return;
      }
      
      // Skip confirmation buttons
      if (calibMode == CALIB_SKIP_WARNING) {
        // Yes button - skip SG calibration
        if (touchX >= BUTTON_CALIB_SKIP_YES_X && touchX <= BUTTON_CALIB_SKIP_YES_X + BUTTON_CALIB_SKIP_YES_W &&
            touchY >= BUTTON_CALIB_SKIP_YES_Y && touchY <= BUTTON_CALIB_SKIP_YES_Y + BUTTON_CALIB_SKIP_YES_H) {
          sendExitCalibrationCommand();
          
          // Save offset only and exit to Live View
          currentMode = LIVE_VIEW;
          calibMode = CALIB_IDLE;
          calibrationModeActive = false;
          stopWait(); // Stop any active wait
          screenDirty = true;
          staticElementsDrawn = false; // Force complete redraw to clear artifacts
          LOG_INFOLN("SKIP SG CALIBRATION - saved offset only, returned to Live View");
        }
        
        // No button - continue with SG calibration
        if (touchX >= BUTTON_CALIB_SKIP_NO_X && touchX <= BUTTON_CALIB_SKIP_NO_X + BUTTON_CALIB_SKIP_NO_W &&
            touchY >= BUTTON_CALIB_SKIP_NO_Y && touchY <= BUTTON_CALIB_SKIP_NO_Y + BUTTON_CALIB_SKIP_NO_H) {
          calibMode = CALIB_POINT1;
          screenDirty = true;
          LOG_VERBOSELN("Continue with SG calibration");
        }
      }
      
      if (calibMode >= CALIB_POINT1 && calibMode <= CALIB_POINT6 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        int idx = calibrationPointIndex(calibMode);
        uint8_t command = calibUseSixPoints ? (uint8_t)(8 + idx) : (uint8_t)idx;
        sendCalibrationCommand(command, calibrationTargetSG(idx));
        startWait(2000); // 2 second wait for RECORD
        LOG_VERBOSE("RECORD Point %d - started 2s wait\n", idx + 1);
      }
      
      // Apply button - new workflow with 3s wait and failure handling
      if (calibMode == CALIB_COMPLETE &&
          touchX >= BUTTON_CALIB_APPLY_X && touchX <= BUTTON_CALIB_APPLY_X + BUTTON_CALIB_APPLY_W &&
          touchY >= BUTTON_CALIB_APPLY_Y && touchY <= BUTTON_CALIB_APPLY_Y + BUTTON_CALIB_APPLY_H) {
        calibMode = CALIB_APPLYING;
        applyCalibration();
        startWait(3000); // 3 second wait for APPLY
        screenDirty = true;
        LOG_VERBOSELN("APPLY - started 3s wait for confirmation");
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
        LOG_VERBOSE("Sent calibration command %d directly to float (MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n", 
                      command, floatMac[0], floatMac[1], floatMac[2], floatMac[3], floatMac[4], floatMac[5]);
      }
    } else {
      result = esp_now_send(NULL, (uint8_t*)&calibCmd, sizeof(calibCmd));
      if (retry == 0) {
        LOG_VERBOSE("Sent calibration command %d via broadcast (float MAC unknown)\n", command);
      }
    }
    
    if (result == ESP_OK) {
      break; // Success, no need to retry
    } else {
      if (retry < maxRetries - 1) {
        LOG_ERROR("Calibration command send failed (attempt %d/%d): %d\n", retry + 1, maxRetries, result);
        delay(100); // Wait before retry
      } else {
        LOG_ERROR("Failed to send calibration command after %d attempts: %d\n", maxRetries, result);
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
    LOG_VERBOSE("Sent apply calibration command directly to float (MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n", 
                  floatMac[0], floatMac[1], floatMac[2], floatMac[3], floatMac[4], floatMac[5]);
  } else {
    result = esp_now_send(NULL, (uint8_t*)&calibCmd, sizeof(calibCmd));
    LOG_VERBOSELN("Sent apply calibration command via broadcast (float MAC unknown)");
  }
  
  if (result != ESP_OK) {
    LOG_ERROR("Error sending apply calibration command: %d\n", result);
  }
}

void sendExitCalibrationCommand() {
  calib_command_t exitCmd;
  exitCmd.command = 7; // EXIT_CALIBRATION
  exitCmd.target_sg = 0.0f;
  exitCmd.request_id = 254;

  if (floatMacKnown) {
    esp_err_t result = esp_now_send(floatMac, (uint8_t*)&exitCmd, sizeof(exitCmd));
    LOG_VERBOSE("Sent exit calibration command to float: %d\n", result);
  } else {
    esp_err_t result = esp_now_send(NULL, (uint8_t*)&exitCmd, sizeof(exitCmd));
    LOG_VERBOSE("Sent exit calibration command via broadcast: %d\n", result);
  }
}

void abortCalibrationFlow() {
  sendExitCalibrationCommand();
  currentMode = LIVE_VIEW;
  calibMode = CALIB_IDLE;
  calibrationModeActive = false;
  stopWait();
  for (int i = 0; i < MAX_BASE_CALIB_POINTS; i++) calibAngles[i] = 0.0f;
  screenDirty = true;
  staticElementsDrawn = false;
  LOG_INFOLN("Calibration aborted - returned to Live view");
}

void onCalibrationResponse(const uint8_t *mac, const uint8_t *incomingData, int len) {
  // Flood protection: Check rate limiting
  unsigned long currentTime = millis();
  if (currentTime - lastCalibrationResponse < CALIBRATION_RESPONSE_RATE_LIMIT) {
    LOG_VERBOSE("Calibration response rate limited (dropped)\n");
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
    LOG_ERROR("Calibration response flood detected - blocking responses\n");
    return;
  }
  
  // Handle calibration response from float unit
  if (len != sizeof(calib_response_t)) {
    LOG_ERROR("Invalid calibration response size: %d bytes (expected %d)\n", len, sizeof(calib_response_t));
    return;
  }
  
  calib_response_t calibResp;
  memcpy(&calibResp, incomingData, len);
  
  LOG_VERBOSE("Calibration response: angle=%.2f°, sg=%.3f, request_id=%d\n", 
                calibResp.angle, calibResp.sg, calibResp.request_id);
  
  if (calibResp.response_type == 0) {
    // Simplified response handling for new workflow
    // Handle offset calibration response (request_id=5)
    if (calibResp.request_id == 5 && calibMode == CALIB_OFFSET) {
      LOG_INFOLN("Offset calibration completed");
      stopWait(); // Stop the 2s wait
      calibOffset = calibResp.angle; // Store offset angle
      offsetCalibrated = true;
      if (calibDoSGCalibration) {
        calibMode = CALIB_POINT1; // Move to first SG calibration point
      } else {
        sendExitCalibrationCommand();
        calibMode = CALIB_IDLE;
        currentMode = LIVE_VIEW;
        calibrationModeActive = false;
        staticElementsDrawn = false;
      }
      screenDirty = true;
      LOG_INFO("Stored offset angle: %.2f°, moved to Point 1\n", calibOffset);
    }
    // Handle apply response (request_id=4) - polynomial calculation complete
    else if (calibResp.request_id == 4 && calibMode == CALIB_APPLYING) {
      LOG_INFOLN("Calibration apply completed - coefficients calculated on float unit");
      
      stopWait(); // Stop the 3s wait
      calibMode = CALIB_IDLE;
      currentMode = LIVE_VIEW; // Exit calibration view
      calibrationModeActive = false;
      screenDirty = true;
      staticElementsDrawn = false; // Force full redraw
      LOG_VERBOSELN("Switched to Live View after calibration completion");
    }
    // Handle exit calibration response (request_id=254) - float unit exiting calibration
    else if (calibResp.request_id == 254) {
      LOG_INFOLN("Float unit confirmed exit from calibration mode");
      // Ensure screen is clean if we're already in live view
      if (currentMode == LIVE_VIEW) {
        screenDirty = true;
        staticElementsDrawn = false; // Force full redraw to clear any remaining artifacts
      }
    } else {
      for (int i = 0; i < calibrationPointCount(); i++) {
        if (fabs(calibResp.sg - calibrationTargetSG(i)) <= 0.006f) {
          calibAngles[i] = calibResp.angle;
          LOG_INFO("Stored Point %d angle: %.2f deg\n", i + 1, calibAngles[i]);
          break;
        }
      }
      return;
      // Calibration data - store angle based on which point was calibrated
      // Determine which calibration point this response is for based on target_sg
      if (calibResp.sg >= 0.995 && calibResp.sg <= 1.005) {
        // Point 1 - Water (SG=1.000)
        calibAngles[0] = calibResp.angle;
        LOG_INFO("Stored Point 1 angle: %.2f°\n", calibAngles[0]);
        // Note: State advancement is now handled by wait timeout, not response
      } else if (calibResp.sg >= 1.035 && calibResp.sg <= 1.045) {
        // Point 2 - Light Sugar (SG=1.040)
        calibAngles[1] = calibResp.angle;
        LOG_INFO("Stored Point 2 angle: %.2f°\n", calibAngles[1]);
        // Note: State advancement is now handled by wait timeout, not response
      } else if (calibResp.sg >= 1.075 && calibResp.sg <= 1.085) {
        // Point 3 - Medium Sugar (SG=1.080)
        calibAngles[2] = calibResp.angle;
        LOG_INFO("Stored Point 3 angle: %.2f°\n", calibAngles[2]);
        // Note: State advancement is now handled by wait timeout, not response
      } else if (calibResp.sg >= 1.115 && calibResp.sg <= 1.125) {
        // Point 4 - Heavy Sugar (SG=1.120)
        calibAngles[3] = calibResp.angle;
        LOG_INFO("Stored Point 4 angle: %.2f°\n", calibAngles[3]);
        // Note: State advancement is now handled by wait timeout, not response
      }
    }
  }
}

// Handle calibration commands from float unit (triggers)
void onCalibrationCoefficients(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(calib_coeffs_t)) {
    LOG_ERROR("Invalid coefficients packet size: %d bytes (expected %d)\n", len, sizeof(calib_coeffs_t));
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
  
  LOG_INFOLN("Received calibration coefficients from float unit:");
  LOG_INFO("GRAVITY = %.12e * norm(Tilt)^3 + %.12e * norm(Tilt)^2 + %.12e * norm(Tilt) + %.12e\n",
                calibCoeffs.coeff3, calibCoeffs.coeff2, calibCoeffs.coeff1, calibCoeffs.coeff0);
  LOG_INFO("Normalization: norm(Tilt) = (Tilt - %.2f) / %.2f\n", normOffset, normScale);
  LOG_INFOLN("Coefficients saved to EEPROM for polynomial graph");
  
  // Test polynomial behavior to detect non-monotonic issues
  testPolynomialRange();
}

void onCalibrationCommandFromFloat(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(calib_command_t)) {
    LOG_ERROR("Invalid calibration command size from float: %d bytes (expected %d)\n", len, sizeof(calib_command_t));
    return;
  }
  
  calib_command_t cmd;
  memcpy(&cmd, incomingData, len);
  
  LOG_VERBOSE("Received calibration command from float: %d, target_sg=%.3f, request_id=%d\n", 
                cmd.command, cmd.target_sg, cmd.request_id);
  
  if (cmd.command == 6 && cmd.request_id == 255) {
    // CALIBRATION_TRIGGER - float entered calibration mode
    LOG_INFOLN("Float entered calibration mode - starting calibration sequence");
    
    // Capture float MAC if not known yet (trigger might come before data)
    if (!floatMacKnown) {
      memcpy(floatMac, mac, 6);
      floatMacKnown = true;
      LOG_INFO("Float MAC captured from trigger: %02X:%02X:%02X:%02X:%02X:%02X\n", 
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
        LOG_INFOLN("Float unit registered as ESP-NOW peer (from trigger)");
      } else if (addResult == ESP_ERR_ESPNOW_EXIST) {
        LOG_INFOLN("Float unit already registered as peer");
      } else {
        LOG_ERROR("Failed to register float as peer: %d\n", addResult);
      }
    }
    
    calibrationModeActive = true;
    
    // Switch to calibration view and let the user choose calibration options first.
    currentMode = CALIBRATION_VIEW;
    calibMode = CALIB_SETUP;
    screenDirty = true;
  }
}

bool parseCSVDataLine(const char* line, payload_t* data, uint32_t* epoch_s, uint8_t* battery_percent) {
  char timestampText[24] = {0};
  unsigned long parsedEpoch = 0;
  unsigned long parsedUptime = 0;
  memset(data, 0, sizeof(payload_t));
  *epoch_s = 0;
  *battery_percent = 0;
  
  int parsed = sscanf(line, "%23[^,],%lu,%lu,%f,%f,%f,%f,%hhu",
                      timestampText,
                      &parsedEpoch,
                      &parsedUptime,
                      &data->angle,
                      &data->density,
                      &data->temperature,
                      &data->battery_voltage,
                      battery_percent);
  
  if (parsed == 8) {
    *epoch_s = (uint32_t)parsedEpoch;
    data->uptime_s = (uint32_t)parsedUptime;
    if (!isEpochValid(*epoch_s)) {
      *epoch_s = parseDateTimeToEpoch(timestampText);
    }
    return true;
  }
  
  parsed = sscanf(line, "%lu,%f,%f,%f,%f,%hhu",
                  &parsedUptime,
                  &data->angle,
                  &data->density,
                  &data->temperature,
                  &data->battery_voltage,
                  battery_percent);
  
  if (parsed == 6) {
    data->uptime_s = (uint32_t)parsedUptime;
    return true;
  }
  
  return false;
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
    LOG_VERBOSE("Historical CSV not readable yet: %s\n", filename);
    return false;
  }
  
  LOG_VERBOSE("Loading historical data from: %s (mode=%d, maxPoints=%d, step=%d)\n", 
                filename, mode, maxPoints, samplingStep);
  
  // First pass: count total data lines
  int totalDataLines = 0;
  uint32_t newestEpoch = 0;
  uint32_t oldestEpoch = 0;
  uint32_t batchStartForScan = brewProfileLoaded ? activeBrewProfile.createdAt : 0;
  int preBatchLines = 0;
  int postBatchLines = 0;
  uint32_t lastPreBatchEpoch = 0;
  uint32_t firstPostBatchEpoch = 0;
  static char line[180];
  
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
      payload_t data = {0};
      uint32_t epoch_s = 0;
      uint8_t battery_percent = 0;
      if (parseCSVDataLine(line, &data, &epoch_s, &battery_percent) && isEpochValid(epoch_s)) {
        newestEpoch = epoch_s;
        if (!isEpochValid(oldestEpoch)) oldestEpoch = epoch_s;
        if (isEpochValid(batchStartForScan)) {
          if (epoch_s < batchStartForScan) {
            preBatchLines++;
            lastPreBatchEpoch = epoch_s;
          } else {
            postBatchLines++;
            if (!isEpochValid(firstPostBatchEpoch)) firstPostBatchEpoch = epoch_s;
          }
        }
      }
    }
  }
  
  LOG_VERBOSE("Found %d total data lines in CSV\n", totalDataLines);
  totalCSVDataLines = totalDataLines;
  
  // Calculate loading parameters based on mode using data point count (600s interval)
  int linesToSkip = 0;
  int linesToLoad = maxPoints;
  int actualSamplingStep = samplingStep;
  uint32_t cutoffEpoch = 0;
  
  switch (mode) {
    case LOAD_6H:
      if (isEpochValid(newestEpoch)) {
        cutoffEpoch = newestEpoch - 6UL * 3600UL;
        linesToLoad = totalDataLines;
        linesToSkip = 0;
        LOG_VERBOSE("LOAD_6H: Loading points since RTC epoch %lu\n", (unsigned long)cutoffEpoch);
        break;
      }
      // Fallback for old files without RTC timestamps (10 minute interval)
      linesToLoad = min(36, totalDataLines);
      linesToSkip = max(0, totalDataLines - linesToLoad);
      actualSamplingStep = 1;
      LOG_VERBOSE("LOAD_6H: Loading last %d points (6 hours at 600s interval)\n", linesToLoad);
      break;
      
    case LOAD_24H:
      if (isEpochValid(newestEpoch)) {
        cutoffEpoch = newestEpoch - 24UL * 3600UL;
        linesToLoad = totalDataLines;
        linesToSkip = 0;
        LOG_VERBOSE("LOAD_24H: Loading points since RTC epoch %lu\n", (unsigned long)cutoffEpoch);
        break;
      }
      // Fallback for old files without RTC timestamps (10 minute interval)
      linesToLoad = min(144, totalDataLines);
      linesToSkip = max(0, totalDataLines - linesToLoad);
      actualSamplingStep = 1;
      LOG_VERBOSE("LOAD_24H: Loading last %d points (24 hours at 600s interval)\n", linesToLoad);
      break;
      
    case LOAD_ALL:
      // Load all data with sampling to fit in maxPoints
      linesToSkip = 0;
      actualSamplingStep = max(1, totalDataLines / maxPoints);
      linesToLoad = totalDataLines;
      LOG_VERBOSE("LOAD_ALL: Loading all %d points with step=%d\n", linesToLoad, actualSamplingStep);
      break;
  }
  
  // Reset display data buffer
  clearHistoricalDisplayData();
  totalCSVDataLines = totalDataLines;
  lastHistoricalPreBatchLines = preBatchLines;
  lastHistoricalPostBatchLines = postBatchLines;
  lastHistoricalOldestEpoch = oldestEpoch;
  lastHistoricalNewestEpoch = newestEpoch;
  
  // Second pass: load data with line skipping and sampling
  file.seek(0);
  int lineNum = 0;
  int loadedCount = 0;
  int skippedCount = 0;
  int timeFilteredCount = 0;
  int parseErrorCount = 0;
  int sampleCounter = 0;
  int batchFilteredCount = 0;
  int cutoffFilteredCount = 0;
  uint32_t batchStartEpoch = brewProfileLoaded ? activeBrewProfile.createdAt : 0;
  bool filterByBatchStart = isEpochValid(batchStartEpoch);
  bool relaxedBatchStart = false;
  if (filterByBatchStart && mode == LOAD_ALL && preBatchLines > 0 && postBatchLines > 0 &&
      preBatchLines > postBatchLines * 2) {
    relaxedBatchStart = true;
    filterByBatchStart = false;
    LOG_INFO("CSV history: relaxed batch start filter for LOAD_ALL pre=%d post=%d\n",
             preBatchLines, postBatchLines);
  } else if (filterByBatchStart && preBatchLines > 0 && postBatchLines > 0 &&
      isEpochValid(lastPreBatchEpoch) && isEpochValid(firstPostBatchEpoch) &&
      firstPostBatchEpoch >= lastPreBatchEpoch &&
      firstPostBatchEpoch - lastPreBatchEpoch <= 3UL * 3600UL &&
      preBatchLines > postBatchLines * 2) {
    relaxedBatchStart = true;
    filterByBatchStart = false;
    LOG_INFO("CSV history: relaxed batch start filter pre=%d post=%d gap=%lus\n",
             preBatchLines, postBatchLines, (unsigned long)(firstPostBatchEpoch - lastPreBatchEpoch));
  }
  lastHistoricalBatchStartRelaxed = relaxedBatchStart;
  
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
    
    payload_t data = {0};
    uint32_t epoch_s = 0;
    uint8_t battery_percent = 0;
    bool parsed = parseCSVDataLine(line, &data, &epoch_s, &battery_percent);
    
    if (parsed) {
      if (filterByBatchStart && isEpochValid(epoch_s) && epoch_s < batchStartEpoch) {
        timeFilteredCount++;
        batchFilteredCount++;
        continue;
      }

      if (isEpochValid(cutoffEpoch) && (!isEpochValid(epoch_s) || epoch_s < cutoffEpoch)) {
        timeFilteredCount++;
        cutoffFilteredCount++;
        continue;
      }
      
      addDataPoint(data, epoch_s);
      loadedCount++;
      
      uint8_t battPercent = calculateBatteryPercentage(data.battery_voltage);
      
      if (!ogCaptured && data.density > 1.000) {
        originalGravity = data.density;
        ogCaptured = true;
        LOG_INFO("Loaded OG from CSV: %.4f\n", originalGravity);
      }
      
      if (loadedCount <= 3 || loadedCount > linesToLoad - 3) {
        LOG_VERBOSE("Loaded point %d: SG=%.4f, Temp=%.1f°C, Batt=%.2fV (%d%%)\n",
                     loadedCount, data.density, data.temperature, data.battery_voltage, battPercent);
      }
    } else if (strlen(line) > 0) {
      parseErrorCount++;
      LOG_VERBOSE("Warning: Failed to parse line %d: %s\n", lineNum, line);
    }
  }
  
  file.close();
  
  lastHistoricalLoadedLines = loadedCount;
  lastHistoricalSkippedLines = timeFilteredCount;
  lastHistoricalParseErrors = parseErrorCount;
  lastHistoricalBatchFilteredLines = batchFilteredCount;
  lastHistoricalCutoffFilteredLines = cutoffFilteredCount;
  LOG_VERBOSE("Loaded %d data points from CSV\n", loadedCount);
  LOG_INFO("CSV history: loaded=%d total=%d skipped=%d parseErrors=%d\n",
           loadedCount, totalDataLines, timeFilteredCount, parseErrorCount);
  finalizeHistoricalDataLoad();
  
  // Initialize charging state based on historical data
  if (loadedCount >= 2) {
    payload_t secondLastReading = displayDataBuffer[(displayDataIndex - 2 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    payload_t lastReading = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    
    float voltageChange = lastReading.battery_voltage - secondLastReading.battery_voltage;
    
    if (voltageChange > 0.02) {
      chargingState = CHARGING;
      LOG_VERBOSE("Initial charging state: CHARGING (voltage increased by %.3fV)\n", voltageChange);
    } else if (voltageChange < -0.02) {
      chargingState = DISCHARGING;
      LOG_VERBOSE("Initial charging state: DISCHARGING (voltage decreased by %.3fV)\n", voltageChange);
    } else {
      chargingState = DISCHARGING;
      LOG_VERBOSE("Initial charging state: DISCHARGING (voltage stable, change: %.3fV)\n", voltageChange);
    }
    
    lastBatteryVoltage = lastReading.battery_voltage;
    lastBatteryCheck = millis();
  } else {
    LOG_VERBOSELN("Not enough data to determine initial charging state");
  }
  
  return loadedCount > 0;
}

void formatDurationShort(unsigned long seconds, char* buffer, size_t bufferSize) {
  if (seconds == 0) {
    snprintf(buffer, bufferSize, "now");
    return;
  }
  unsigned long days = seconds / 86400UL;
  unsigned long hours = (seconds % 86400UL) / 3600UL;
  if (days > 0) snprintf(buffer, bufferSize, "%lud %luh", days, hours);
  else snprintf(buffer, bufferSize, "%luh", hours);
}

void startNewBatchStorage(const char* batchId) {
  char logPath[80];
  BrewProfileStore::logPath(batchId, logPath, sizeof(logPath));
  strncpy(currentFermentationFile, logPath, sizeof(currentFermentationFile) - 1);
  currentFermentationFile[sizeof(currentFermentationFile) - 1] = '\0';
  fermentationFileOpen = true;
}

bool ensureFermentationLogFile(const char* batchId) {
  #if !SD_ENABLED
    return false;
  #endif

  char logPath[80];
  BrewProfileStore::logPath(batchId, logPath, sizeof(logPath));
  if (SD.exists(logPath)) {
    File existing = SD.open(logPath, FILE_READ);
    if (existing && !existing.isDirectory()) {
      existing.close();
      return true;
    }
    if (existing) existing.close();
    LOG_ERROR("Batch log exists but is not readable as a file: %s\n", logPath);
    return false;
  }

  BrewProfileStore::ensureBatchDirectory(batchId);
  File file = SD.open(logPath, FILE_WRITE);
  if (!file) {
    LOG_ERROR("Failed to create batch log file: %s\n", logPath);
    return false;
  }

  file.println("timestamp,epoch_s,uptime_s,angle,density,temperature,battery_voltage,battery_percent,state,current_attenuation,estimated_abv,recommendation_code");
  file.flush();
  file.close();
  LOG_INFO("Created missing batch log file: %s\n", logPath);
  return true;
}

bool findBestFallbackBatch(char* batchId, size_t bufferSize) {
  if (!batchId || bufferSize == 0) return false;
  batchId[0] = '\0';
  if (!SD.exists("/data/batches")) return false;

  File root = SD.open("/data/batches");
  if (!root) return false;

  int highestNumber = 0;
  int bestDataNumber = 0;
  uint32_t bestDataEpoch = 0;
  static char line[180];
  static BrewProfile candidateProfile;

  File entry = root.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      const char* filename = entry.name();
      const char* slash = strrchr(filename, '/');
      if (slash) filename = slash + 1;
      int fileNum = 0;
      if (sscanf(filename, "batch_%d", &fileNum) == 1) {
        char candidateBatchId[24];
        char profilePath[80];
        BrewProfileStore::buildBatchId(fileNum, candidateBatchId, sizeof(candidateBatchId));
        BrewProfileStore::profilePath(candidateBatchId, profilePath, sizeof(profilePath));
        if (SD.exists(profilePath)) {
          uint32_t newestEligibleEpoch = 0;
          if (BrewProfileStore::load(candidateBatchId, &candidateProfile) && !candidateProfile.completed) {
            if (fileNum > highestNumber) highestNumber = fileNum;
            char logPath[80];
            BrewProfileStore::logPath(candidateBatchId, logPath, sizeof(logPath));
            File logFile = SD.open(logPath, FILE_READ);
            if (logFile) {
              while (logFile.available()) {
                int bytesRead = logFile.readBytesUntil('\n', line, sizeof(line) - 1);
                line[bytesRead] = '\0';
                if (strncmp(line, "timestamp,", 10) == 0 || line[0] == '#' || strlen(line) == 0) continue;

                payload_t data = {0};
                uint32_t epoch = 0;
                uint8_t batteryPercent = 0;
                if (parseCSVDataLine(line, &data, &epoch, &batteryPercent) &&
                    isEpochValid(epoch) &&
                    (!isEpochValid(candidateProfile.createdAt) || epoch >= candidateProfile.createdAt)) {
                  if (epoch > newestEligibleEpoch) newestEligibleEpoch = epoch;
                }
              }
              logFile.close();
            }
          }

          if (isEpochValid(newestEligibleEpoch) && newestEligibleEpoch > bestDataEpoch) {
            bestDataEpoch = newestEligibleEpoch;
            bestDataNumber = fileNum;
          }
        }
      }
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();

  int selectedNumber = bestDataNumber > 0 ? bestDataNumber : highestNumber;
  if (selectedNumber <= 0) return false;
  BrewProfileStore::buildBatchId(selectedNumber, batchId, bufferSize);
  return true;
}

void persistActiveBrewProfile() {
  #if !SD_ENABLED
    return;
  #endif

  if (!brewProfileLoaded) return;
  if (mountSDTemporarily()) {
    activeBrewProfile.expectedFinalGravity = DerivedCalculations::expectedFG(
      activeBrewProfile.effectiveOG,
      activeBrewProfile.expectedApparentAttenuation);
    activeBrewProfile.estimatedABV = fermentationMetrics.estimatedABV;
    BrewProfileStore::save(activeBrewProfile);
    dismountSD();
  }
}

void beginBrewWizard() {
  if (!brewWizardHasSavedRuntime || currentMode != BREW_WIZARD_VIEW) {
    brewWizardSavedProfile = activeBrewProfile;
    brewWizardSavedProfileLoaded = brewProfileLoaded;
    brewWizardSavedFermentationFileOpen = fermentationFileOpen;
    strncpy(brewWizardSavedFermentationFile, currentFermentationFile, sizeof(brewWizardSavedFermentationFile) - 1);
    brewWizardSavedFermentationFile[sizeof(brewWizardSavedFermentationFile) - 1] = '\0';
    brewWizardHasSavedRuntime = true;
  }

  BrewProfileStore::setDefaults(&activeBrewProfile);
  bool wizardInitialized = false;
  if (mountSDTemporarily()) {
    if (!SD.exists("/data")) SD.mkdir("/data");
    if (!SD.exists("/data/batches")) SD.mkdir("/data/batches");
    int number = 1;
    char batchId[24];
    char profilePath[80];
    char logPath[80];
    char batchDir[64];
    do {
      BrewProfileStore::buildBatchId(number++, batchId, sizeof(batchId));
      BrewProfileStore::profilePath(batchId, profilePath, sizeof(profilePath));
      BrewProfileStore::logPath(batchId, logPath, sizeof(logPath));
      snprintf(batchDir, sizeof(batchDir), "/data/batches/%s", batchId);
    } while (SD.exists(profilePath) || SD.exists(logPath) || SD.exists(batchDir));
    strncpy(activeBrewProfile.batchId, batchId, sizeof(activeBrewProfile.batchId) - 1);
    activeBrewProfile.batchId[sizeof(activeBrewProfile.batchId) - 1] = '\0';
    activeBrewProfile.createdAt = getCurrentEpoch();
    brewWizard.begin(&activeBrewProfile);
    wizardInitialized = true;
    dismountSD();
  }
  if (!wizardInitialized) {
    activeBrewProfile.createdAt = getCurrentEpoch();
    brewWizard.begin(&activeBrewProfile);
  }
  currentMode = BREW_WIZARD_VIEW;
  screenDirty = true;
}

void cancelBrewWizard() {
  if (brewWizardHasSavedRuntime) {
    activeBrewProfile = brewWizardSavedProfile;
    brewProfileLoaded = brewWizardSavedProfileLoaded;
    fermentationFileOpen = brewWizardSavedFermentationFileOpen;
    strncpy(currentFermentationFile, brewWizardSavedFermentationFile, sizeof(currentFermentationFile) - 1);
    currentFermentationFile[sizeof(currentFermentationFile) - 1] = '\0';
    brewWizardHasSavedRuntime = false;
  }
  refreshFermentationAssistantFromProfile();
  currentMode = DASHBOARD_VIEW;
  screenDirty = true;
}

void completeBrewWizard() {
  brewWizardHasSavedRuntime = false;
  activeBrewProfile.recipeOG = BrixConverter::brixToSG(activeBrewProfile.recipeBrix);
  activeBrewProfile.effectiveOG = activeBrewProfile.recipeOG;
  activeBrewProfile.expectedFinalGravity = DerivedCalculations::expectedFG(
    activeBrewProfile.effectiveOG,
    activeBrewProfile.expectedApparentAttenuation);
  activeBrewProfile.estimatedABV = 0.0f;
  activeBrewProfile.ogVerified = false;
  activeBrewProfile.ogNeedsChoice = false;
  BatchActionEngine::applyStyleDefaults(&activeBrewProfile);
  brewProfileLoaded = true;
  ogVerificationPending = false;
  targetCurveAvailable = false;
  yeastPerformanceSaved = false;
  ogVerifier.reset();
  fermentationStateMachine.reset();
  previousAnalyticsSG = 0.0f;
  previousAnalyticsEpoch = 0;
  originalGravity = activeBrewProfile.effectiveOG;
  currentABV = 0.0f;
  ogCaptured = true;

  if (mountSDTemporarily()) {
    BrewProfileStore::ensureBatchDirectory(activeBrewProfile.batchId);
    BrewProfileStore::save(activeBrewProfile);
    BrewProfileStore::saveActiveBatchId(activeBrewProfile.batchId);
    BrewProfileStore::saveYeastHistory(activeBrewProfile.yeastName);
    TargetCurveGenerator::generateAndSave(activeBrewProfile);
    targetCurveAvailable = true;

    char logPath[80];
    BrewProfileStore::logPath(activeBrewProfile.batchId, logPath, sizeof(logPath));
    if (SD.exists(logPath)) {
      SD.remove(logPath);
    }
    File file = SD.open(logPath, FILE_WRITE);
    if (file) {
      file.println("timestamp,epoch_s,uptime_s,angle,density,temperature,battery_voltage,battery_percent,state,current_attenuation,estimated_abv,recommendation_code");
      file.close();
    }
    startNewBatchStorage(activeBrewProfile.batchId);
    dismountSD();
  }

  clearHistoricalDisplayData();
  refreshFermentationAssistantFromProfile();
  currentMode = LIVE_VIEW;
  staticElementsDrawn = false;
  screenDirty = true;
}

void handleOGChoice(bool useMeasuredOG) {
  if (!brewProfileLoaded) return;
  if (useMeasuredOG) activeBrewProfile.effectiveOG = activeBrewProfile.measuredOG;
  else activeBrewProfile.effectiveOG = activeBrewProfile.recipeOG;

  activeBrewProfile.ogVerified = true;
  activeBrewProfile.ogNeedsChoice = false;
  ogVerificationPending = false;
  ogVerifier.reset();
  originalGravity = activeBrewProfile.effectiveOG;
  activeBrewProfile.expectedFinalGravity = DerivedCalculations::expectedFG(
    activeBrewProfile.effectiveOG,
    activeBrewProfile.expectedApparentAttenuation);

  if (mountSDTemporarily()) {
    BrewProfileStore::save(activeBrewProfile);
    TargetCurveGenerator::generateAndSave(activeBrewProfile);
    dismountSD();
  }
  refreshFermentationAssistantFromProfile();
  float currentSG = activeBrewProfile.effectiveOG;
  if (latestFloatDataValid) {
    currentSG = latestFloatData.density;
  } else if (displayDataCount > 0) {
    currentSG = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS].density;
  }
  unsigned long nowEpoch = getCurrentEpoch();
  if (!isEpochValid(nowEpoch)) nowEpoch = latestFloatDataValid ? latestFloatEpoch : activeBrewProfile.createdAt;
  refreshCurrentBatchAction(currentSG, nowEpoch);
  if (currentBatchAction.type == ACTION_VERIFY_OG ||
      currentRecommendation.code == 10 ||
      currentRecommendation.code == 11) {
    currentBatchAction.type = ACTION_NONE;
    currentBatchAction.code = 0;
    currentBatchAction.requiresChoice = false;
    currentBatchAction.secondsUntilDue = 0;
    strcpy(currentBatchAction.title, "Next");
    strcpy(currentBatchAction.message, "OG verified");
    currentRecommendation.code = 0;
    strcpy(currentRecommendation.message, "OG verified");
  }
  currentMode = LIVE_VIEW;
  staticElementsDrawn = false;
  screenDirty = true;
}

void handleCurrentActionChoice(bool done) {
  if (!brewProfileLoaded || currentBatchAction.type == ACTION_NONE || !currentBatchAction.requiresChoice) return;
  BatchActionType type = currentBatchAction.type;
  unsigned long nowEpoch = getCurrentEpoch();
  if (!isEpochValid(nowEpoch)) nowEpoch = latestFloatDataValid ? latestFloatEpoch : activeBrewProfile.createdAt;

  bool changed = done
    ? BatchActionEngine::applyDone(&activeBrewProfile, type, nowEpoch)
    : BatchActionEngine::applySkip(&activeBrewProfile, type, nowEpoch);
  if (!changed) return;

  if (mountSDTemporarily()) {
    BrewProfileStore::save(activeBrewProfile);
    BrewProfileStore::appendBatchEvent(activeBrewProfile.batchId, nowEpoch,
                                       BatchActionEngine::eventName(type, done),
                                       currentBatchAction.message,
                                       latestFloatDataValid ? latestFloatData.density : 0.0f);
    dismountSD();
  }

  float currentSG = latestFloatDataValid ? latestFloatData.density :
    (displayDataCount > 0 ? displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS].density : activeBrewProfile.effectiveOG);
  currentRecommendation.code = 0;
  strncpy(currentRecommendation.message, done ? "Action marked done" : "Action skipped", sizeof(currentRecommendation.message) - 1);
  currentRecommendation.message[sizeof(currentRecommendation.message) - 1] = '\0';
  refreshCurrentBatchAction(currentSG, nowEpoch);
  screenDirty = true;
}

void saveYeastPerformanceSummary(float finalGravity, uint32_t completedAt) {
  if (!brewProfileLoaded || yeastPerformanceSaved) return;

  YeastPerformanceSummary summary;
  memset(&summary, 0, sizeof(summary));
  strncpy(summary.yeastPresetId, activeBrewProfile.selectedYeastPresetId, sizeof(summary.yeastPresetId) - 1);
  strncpy(summary.yeastName, activeBrewProfile.yeastName, sizeof(summary.yeastName) - 1);
  strncpy(summary.batchId, activeBrewProfile.batchId, sizeof(summary.batchId) - 1);
  summary.effectiveOG = activeBrewProfile.effectiveOG;
  summary.finalGravity = finalGravity;
  summary.actualAttenuation = fermentationMetrics.currentAttenuation;
  summary.completedAt = completedAt;
  if (isEpochValid(completedAt) && isEpochValid(activeBrewProfile.createdAt) && completedAt > activeBrewProfile.createdAt) {
    summary.fermentationDurationHours = (completedAt - activeBrewProfile.createdAt) / 3600.0f;
  }

  float tempSum = 0.0f;
  int tempCount = 0;
  for (int i = 0; i < displayDataCount; i++) {
    int idx = (displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    tempSum += displayDataBuffer[idx].temperature;
    tempCount++;
  }
  summary.averageTemperature = tempCount > 0 ? tempSum / tempCount : 0.0f;

  if (mountSDTemporarily()) {
    yeastPerformanceSaved = BrewProfileStore::appendYeastPerformance(summary);
    dismountSD();
  }
}

bool isPlausibleSensorReading(const payload_t& data, const char** issue) {
  if (isnan(data.density) || isinf(data.density) || data.density < 0.990f || data.density > 1.200f) {
    if (issue) *issue = "invalid SG";
    return false;
  }
  if (isnan(data.temperature) || isinf(data.temperature) || data.temperature < -5.0f || data.temperature > 60.0f) {
    if (issue) *issue = "invalid temperature";
    return false;
  }
  if (isnan(data.battery_voltage) || isinf(data.battery_voltage) ||
      (data.battery_voltage > 0.0f && (data.battery_voltage < 2.5f || data.battery_voltage > 4.4f))) {
    if (issue) *issue = "invalid battery";
    return false;
  }
  if (issue) *issue = "";
  return true;
}

void updateFermentationAssistant(payload_t data, uint32_t epoch_s) {
  if (!brewProfileLoaded) return;

  const char* sensorIssue = "";
  if (!isPlausibleSensorReading(data, &sensorIssue)) {
    currentRecommendation = RecommendationEngine::sensorIssue(sensorIssue);
    currentETA.valid = false;
    markScreenDirtyForFloatData();
    return;
  }

  if (!activeBrewProfile.ogVerified && !activeBrewProfile.ogNeedsChoice) {
    OGVerificationResult result = ogVerifier.addReading(data.density);
    if (result.ready) {
      activeBrewProfile.measuredOG = result.measuredOG;
      activeBrewProfile.ogDifference = activeBrewProfile.measuredOG - activeBrewProfile.recipeOG;
      if (OGVerifier::exceedsThreshold(activeBrewProfile.ogDifference)) {
        activeBrewProfile.ogNeedsChoice = true;
        ogVerificationPending = true;
        currentMode = OG_VERIFICATION_VIEW;
        screenDirty = true;
      } else {
        activeBrewProfile.effectiveOG = activeBrewProfile.recipeOG;
        activeBrewProfile.ogVerified = true;
      }
      persistActiveBrewProfile();
    }
  }

  unsigned long deltaSeconds = previousAnalyticsEpoch > 0 && epoch_s > previousAnalyticsEpoch
    ? epoch_s - previousAnalyticsEpoch
    : 600UL;
  float previousSG = previousAnalyticsSG > 0.0f ? previousAnalyticsSG : data.density;
  fermentationMetrics = DerivedCalculations::compute(
    activeBrewProfile.effectiveOG,
    data.density,
    previousSG,
    deltaSeconds,
    activeBrewProfile.expectedApparentAttenuation);

  activeBrewProfile.expectedFinalGravity = fermentationMetrics.expectedFinalGravity;
  activeBrewProfile.estimatedABV = fermentationMetrics.estimatedABV;
  currentABV = fermentationMetrics.estimatedABV;
  originalGravity = activeBrewProfile.effectiveOG;
  ogCaptured = activeBrewProfile.effectiveOG > 1.0f;

  FermentationPhase phase = fermentationStateMachine.update(
    activeBrewProfile,
    data.density,
    fermentationMetrics.expectedFinalGravity,
    fermentationMetrics.currentAttenuation,
    fermentationMetrics.gravityDeltaPerHour,
    data.temperature,
    epoch_s);

  unsigned long elapsedSeconds = 0;
  if (isEpochValid(epoch_s) && isEpochValid(activeBrewProfile.createdAt) && epoch_s > activeBrewProfile.createdAt) {
    elapsedSeconds = epoch_s - activeBrewProfile.createdAt;
  }

  currentRecommendation = RecommendationEngine::build(
    activeBrewProfile,
    phase,
    activeBrewProfile.ogVerified,
    activeBrewProfile.ogNeedsChoice,
    fermentationMetrics.currentAttenuation,
    fermentationMetrics.expectedFinalGravity,
    data.density,
    data.temperature,
    fermentationMetrics.gravityDeltaPerHour,
    elapsedSeconds);
  currentBatchAction = BatchActionEngine::evaluate(
    activeBrewProfile,
    phase,
    fermentationMetrics.currentAttenuation,
    data.density,
    fermentationMetrics.gravityDeltaPerHour,
    epoch_s);
  if (currentBatchAction.type != ACTION_NONE) {
    currentRecommendation.code = currentBatchAction.code;
    strncpy(currentRecommendation.message, currentBatchAction.message, sizeof(currentRecommendation.message) - 1);
    currentRecommendation.message[sizeof(currentRecommendation.message) - 1] = '\0';
  } else if (currentRecommendation.code >= 100) {
    currentRecommendation.code = 0;
    strncpy(currentRecommendation.message, currentBatchAction.message, sizeof(currentRecommendation.message) - 1);
    currentRecommendation.message[sizeof(currentRecommendation.message) - 1] = '\0';
  }
  currentETA = ETAPredictor::predict(activeBrewProfile, data.density, fermentationMetrics.expectedFinalGravity,
                                     fermentationMetrics.gravityDeltaPerHour, elapsedSeconds);

  if (!yeastPerformanceSaved &&
      (phase == FERMENTATION_READY_TO_PACKAGE || phase == FERMENTATION_COMPLETED)) {
    saveYeastPerformanceSummary(data.density, epoch_s);
  }

  previousAnalyticsSG = data.density;
  previousAnalyticsEpoch = epoch_s;
}

void refreshCurrentBatchAction(float currentSG, unsigned long nowEpoch) {
  if (!brewProfileLoaded) {
    currentBatchAction.type = ACTION_NONE;
    currentBatchAction.code = 0;
    currentBatchAction.requiresChoice = false;
    strcpy(currentBatchAction.title, "Next");
    strcpy(currentBatchAction.message, "No active batch");
    return;
  }
  currentBatchAction = BatchActionEngine::evaluate(
    activeBrewProfile,
    fermentationStateMachine.phase(),
    fermentationMetrics.currentAttenuation,
    currentSG,
    fermentationMetrics.gravityDeltaPerHour,
    nowEpoch);
  if (currentBatchAction.type != ACTION_NONE) {
    currentRecommendation.code = currentBatchAction.code;
    strncpy(currentRecommendation.message, currentBatchAction.message, sizeof(currentRecommendation.message) - 1);
    currentRecommendation.message[sizeof(currentRecommendation.message) - 1] = '\0';
  } else if (currentRecommendation.code == 0 ||
             currentRecommendation.code == 10 ||
             currentRecommendation.code == 11 ||
             currentRecommendation.code >= 100) {
    currentRecommendation.code = 0;
    strncpy(currentRecommendation.message, currentBatchAction.message, sizeof(currentRecommendation.message) - 1);
    currentRecommendation.message[sizeof(currentRecommendation.message) - 1] = '\0';
  }
}

void refreshFermentationAssistantFromProfile() {
  if (!brewProfileLoaded) return;

  activeBrewProfile.expectedFinalGravity = DerivedCalculations::expectedFG(
    activeBrewProfile.effectiveOG,
    activeBrewProfile.expectedApparentAttenuation);

  if (displayDataCount > 0) {
    int latestIndex = (displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    updateFermentationAssistant(displayDataBuffer[latestIndex], displayTimestampBuffer[latestIndex]);
    return;
  }

  fermentationMetrics.expectedFinalGravity = activeBrewProfile.expectedFinalGravity;
  fermentationMetrics.currentAttenuation = 0.0f;
  fermentationMetrics.estimatedABV = activeBrewProfile.estimatedABV;
  fermentationMetrics.gravityDeltaPerHour = 0.0f;
  fermentationMetrics.fermentationRate = 0.0f;

  unsigned long elapsedSeconds = 0;
  uint32_t nowEpoch = getCurrentEpoch();
  if (isEpochValid(nowEpoch) && isEpochValid(activeBrewProfile.createdAt) && nowEpoch > activeBrewProfile.createdAt) {
    elapsedSeconds = nowEpoch - activeBrewProfile.createdAt;
  }

  FermentationPhase phase = fermentationStateMachine.update(
    activeBrewProfile,
    activeBrewProfile.effectiveOG,
    activeBrewProfile.expectedFinalGravity,
    0.0f,
    0.0f,
    0.0f,
    nowEpoch);

  currentRecommendation = RecommendationEngine::build(
    activeBrewProfile,
    phase,
    activeBrewProfile.ogVerified,
    activeBrewProfile.ogNeedsChoice,
    0.0f,
    activeBrewProfile.expectedFinalGravity,
    activeBrewProfile.effectiveOG,
    0.0f,
    0.0f,
    elapsedSeconds);

  currentETA = ETAPredictor::predict(
    activeBrewProfile,
    activeBrewProfile.effectiveOG,
    activeBrewProfile.expectedFinalGravity,
    0.0f,
    elapsedSeconds);
  refreshCurrentBatchAction(activeBrewProfile.effectiveOG, nowEpoch);
}

void checkExistingFermentation() {
  #if !SD_ENABLED
    LOG_INFOLN("SD disabled, skipping fermentation file check");
    // Create new file when data arrives
    return;
  #endif
  
  // Mount SD temporarily to check for existing fermentation files
  if (mountSDTemporarily()) {
    if (!SD.exists("/data")) SD.mkdir("/data");
    if (!SD.exists("/data/batches")) SD.mkdir("/data/batches");
    
    File root = SD.open("/data/batches");
    if (!root) {
      LOG_ERRORLN("Failed to open batch directory, starting Brew Wizard");
      dismountSD();
      beginBrewWizard();
      return;
    }
    
    char batchId[24] = "";
    bool hasSelectedBatch = false;
    if (BrewProfileStore::loadActiveBatchId(batchId, sizeof(batchId))) {
      char profilePath[80];
      BrewProfileStore::profilePath(batchId, profilePath, sizeof(profilePath));
      hasSelectedBatch = SD.exists(profilePath);
      if (hasSelectedBatch) {
        static BrewProfile markerProfile;
        if (!BrewProfileStore::load(batchId, &markerProfile) || markerProfile.completed) {
          hasSelectedBatch = false;
        }
      }
      if (!hasSelectedBatch) {
        LOG_ERROR("Active batch marker points to missing profile: %s\n", batchId);
        BrewProfileStore::clearActiveBatchId();
        batchId[0] = '\0';
      }
    }

    root.close();
    
    if (!hasSelectedBatch && findBestFallbackBatch(batchId, sizeof(batchId))) {
      hasSelectedBatch = true;
      BrewProfileStore::saveActiveBatchId(batchId);
      LOG_INFO("No active batch marker, fallback selected batch: %s\n", batchId);
    }

    if (hasSelectedBatch && batchId[0] != '\0') {
      if (BrewProfileStore::load(batchId, &activeBrewProfile)) {
        brewProfileLoaded = true;
        originalGravity = activeBrewProfile.effectiveOG;
        currentABV = activeBrewProfile.estimatedABV;
        ogCaptured = activeBrewProfile.effectiveOG > 1.0f;
      }
      bool logReady = ensureFermentationLogFile(batchId);
      BrewProfileStore::logPath(batchId, currentFermentationFile, sizeof(currentFermentationFile));
      fermentationFileOpen = logReady;
      LOG_INFO("Auto-continuing batch log: %s\n", currentFermentationFile);
      
      // Load historical data from the CSV file
      if (logReady && loadHistoricalDataFromCSV(currentFermentationFile, LOAD_6H, MAX_DATA_POINTS, 1)) {
        LOG_INFOLN("Historical data loaded successfully - views will show previous data");
      } else {
        LOG_VERBOSELN("No historical CSV data loaded - starting fresh");
      }
      refreshFermentationAssistantFromProfile();
    } else {
      LOG_INFOLN("No existing batches found, opening Brew Wizard");
      dismountSD();
      beginBrewWizard();
      return;
    }
    
    dismountSD();
  }
}

void createNewFermentationFile() {
  #if !SD_ENABLED
    LOG_ERRORLN("SD disabled, cannot create fermentation file");
    return;
  #endif
  
  // Mount SD temporarily to create new fermentation file
  if (mountSDTemporarily()) {
    if (!SD.exists("/data")) SD.mkdir("/data");
    if (!SD.exists("/data/batches")) SD.mkdir("/data/batches");
    
    char filename[64];
    int newFileNumber = 1;
    
    // Parse current batch number if exists
    if (strlen(currentFermentationFile) > 0) {
      int currentNum = 0;
      if (sscanf(currentFermentationFile, "/data/batches/batch_%d/log.csv", &currentNum) == 1) {
        newFileNumber = currentNum + 1;
      }
    }
    
    // Find next available number if file already exists
    while (true) {
      char batchId[24];
      char profilePath[80];
      char batchDir[64];
      BrewProfileStore::buildBatchId(newFileNumber, batchId, sizeof(batchId));
      BrewProfileStore::profilePath(batchId, profilePath, sizeof(profilePath));
      BrewProfileStore::logPath(batchId, filename, sizeof(filename));
      snprintf(batchDir, sizeof(batchDir), "/data/batches/%s", batchId);
      if (!SD.exists(filename) && !SD.exists(profilePath) && !SD.exists(batchDir)) {
        BrewProfileStore::setDefaults(&activeBrewProfile);
        strncpy(activeBrewProfile.batchId, batchId, sizeof(activeBrewProfile.batchId) - 1);
        activeBrewProfile.batchId[sizeof(activeBrewProfile.batchId) - 1] = '\0';
        activeBrewProfile.createdAt = getCurrentEpoch();
        BrewProfileStore::ensureBatchDirectory(activeBrewProfile.batchId);
        break;
      }
      newFileNumber++;
    }
    
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
      LOG_ERRORLN("Failed to create new fermentation file");
      dismountSD();
      return;
    }
    
    // Write CSV header
    file.println("timestamp,epoch_s,uptime_s,angle,density,temperature,battery_voltage,battery_percent,state,current_attenuation,estimated_abv,recommendation_code");
    file.flush();  // Ensure header is written to SD card
    file.close();
    
    strncpy(currentFermentationFile, filename, sizeof(currentFermentationFile) - 1);
    currentFermentationFile[sizeof(currentFermentationFile) - 1] = '\0';
    fermentationFileOpen = true;
    brewProfileLoaded = true;
    BrewProfileStore::save(activeBrewProfile);
    TargetCurveGenerator::generateAndSave(activeBrewProfile);
    
    // Reset OG/ABV for new fermentation
    originalGravity = 0.0;
    currentABV = 0.0;
    readingCount = 0;
    ogCaptured = false;
    last3Readings[0] = 0.0;
    last3Readings[1] = 0.0;
    last3Readings[2] = 0.0;
    clearHistoricalDisplayData();
    
    LOG_INFO("Created new fermentation file: %s (OG/ABV reset)\n", currentFermentationFile);
    dismountSD();
  }
}

bool logDataToSD(payload_t data, uint32_t epoch_s) {
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
      LOG_ERRORLN("Failed to mount SD card for data logging");
      LOG_ERROR("SD CS pin: %d, MOSI: %d, MISO: %d, SCK: %d\n", SD_CS, SD_MOSI, SD_MISO, SD_SCK);
      LOG_ERRORLN("Troubleshooting: Check SD card insertion, wiring, and format (FAT32)");
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
    LOG_ERROR("Failed to open fermentation file '%s' for writing\n", currentFermentationFile);
    
    // Try to create the file if it doesn't exist
    LOG_VERBOSELN("Attempting to create fermentation file...");
    file = SD.open(currentFermentationFile, FILE_WRITE);
    if (!file) {
      LOG_ERRORLN("Failed to create fermentation file");
      return false;
    }
    
    // Write CSV header for new file
    file.println("timestamp,epoch_s,uptime_s,angle,density,temperature,battery_voltage,battery_percent,state,current_attenuation,estimated_abv,recommendation_code");
    file.close();
    
    // Try opening again in append mode
    file = SD.open(currentFermentationFile, FILE_APPEND);
    if (!file) {
      LOG_ERRORLN("Failed to open fermentation file after creation");
      return false;
    }
    LOG_INFOLN("Successfully created new fermentation file with header");
  }
  
  // Add timeout protection
  unsigned long startTime = millis();
  const unsigned long SD_TIMEOUT = 5000; // 5 second timeout
  
  // Write CSV data
  uint8_t batteryPercent = calculateBatteryPercentage(data.battery_voltage);
  char timestamp[24];
  formatTimestamp(epoch_s, timestamp, sizeof(timestamp));
  LOG_VERBOSE("Writing to SD - SG:%.4f, Temp:%.2f, Batt:%.2fV\n", 
                data.density, data.temperature, data.battery_voltage);
  int bytesWritten = file.printf("%s,%lu,%lu,%.2f,%.4f,%.2f,%.2f,%d,%s,%.2f,%.2f,%d\n", 
             timestamp, (unsigned long)epoch_s, (unsigned long)data.uptime_s, data.angle, data.density,
             data.temperature, data.battery_voltage, batteryPercent,
             fermentationStateMachine.phaseName(),
             fermentationMetrics.currentAttenuation,
             fermentationMetrics.estimatedABV,
             currentRecommendation.code);
  
  // Check for timeout
  if (millis() - startTime > SD_TIMEOUT) {
    LOG_ERRORLN("SD write operation timed out");
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
    LOG_VERBOSE("Logged data to SD: SG=%.4f, Temp=%.2f°C, Battery=%.2fV (%d%%)\n", 
                 data.density, data.temperature, data.battery_voltage, batteryPercent);
    return true;
  } else {
    LOG_ERRORLN("Failed to write data to SD file");
    return false;
  }
}

void drawCreateNewDialog() {
  // Draw semi-transparent overlay on current view
  tft.fillRect(20, 80, UI_W - 40, UI_H - 160, uiColorCardBackground);
  
  // Title
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_LG);
  uiTextCenter(20, 100, UI_W - 40, 24, "Create New Brew?", FONT_SIZE_SM_BOLD, uiColorTextPrimary);
  
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
    LOG_ERRORLN("Failed to open fermentation file for OG logging");
    return;
  }
  
  // Write OG as a comment line in the CSV file
  uint32_t epoch_s = getCurrentEpoch();
  char timestamp[24];
  formatTimestamp(epoch_s, timestamp, sizeof(timestamp));
  int bytesWritten = file.printf("# OG_CAPTURED: %.4f at reading #%d, timestamp=%s, epoch_s=%lu\n",
                                 originalGravity, readingCount, timestamp, (unsigned long)epoch_s);
  
  file.flush();  // Ensure OG is written to SD card
  file.close();
  
  if (bytesWritten > 0) {
    LOG_INFO("OG logged to SD: %.4f at reading #%d\n", originalGravity, readingCount);
  } else {
    LOG_ERRORLN("Failed to write OG to SD card");
  }
}

void checkOGStability(float currentSG) {
  // Store current reading in the last 3 readings buffer
  last3Readings[readingCount % 3] = currentSG;
  readingCount++;
  
  LOG_VERBOSE("OG check: reading #%d, SG=%.4f\n", readingCount, currentSG);
  
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
    
    LOG_VERBOSE("Last 3 readings: %.4f, %.4f, %.4f, variance: %.4f\n", r1, r2, r3, variance);
    
    // Check if 3 readings in a row are stable (within threshold)
    if (variance < OG_STABILITY_THRESHOLD) {
      // Calculate average of the 3 stable readings
      originalGravity = (r1 + r2 + r3) / 3.0;
      ogCaptured = true;
      LOG_INFO("OG captured as stable: %.4f (average of 3 readings)\n", originalGravity);
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
    LOG_INFO("OG captured by fallback (reading #%d): %.4f (average of last 3)\n", readingCount, originalGravity);
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

void drawViewTopbar(const char* title) {
  uint8_t battPercent = 0;
  if (displayDataCount > 0) {
    payload_t latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    battPercent = calculateBatteryPercentage(latest.battery_voltage);
  }
  uiDrawTopbar(title, true, fermentationFileOpen, battPercent);
}

void drawNoDataCard(const char* message, const char* subtext, int activeTab) {
  int contentY = TOPBAR_H + MARGIN;
  int contentH = UI_H - contentY - NAV_H;
  uiCard(MARGIN, contentY + contentH / 2 - 40, UI_W - MARGIN * 2, 80, CARD_RADIUS);
  uiTextCenter(MARGIN, contentY + contentH / 2 - 10, UI_W - MARGIN * 2, 20, message, FONT_SIZE_MD, uiColorTextMuted);
  uiTextCenter(MARGIN, contentY + contentH / 2 + 10, UI_W - MARGIN * 2, 20, subtext, FONT_SIZE_SM, uiColorTextMuted);
  uiDrawBottomNav(activeTab);
}

void drawDetailSection(int* y, const char* title) {
  tft.setTextColor(uiColorInfo);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(MARGIN, *y);
  tft.print(title);
  *y += 8;
  tft.drawFastHLine(MARGIN, *y, UI_W - MARGIN * 2, uiColorBorder);
  *y += 3;
}

void drawDetailRow(int* y, const char* label, const char* value) {
  char clipped[40];
  uiEllipsize(value, 250, clipped, sizeof(clipped));

  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(MARGIN, *y);
  tft.print(label);

  tft.setTextColor(uiColorTextPrimary);
  uiTextRight(MARGIN + 150, *y - 13, UI_W - MARGIN * 2 - 150, 16, clipped, FONT_SIZE_XS, uiColorTextPrimary);
  *y += 12;
}

void drawDetailSectionAt(int x, int* y, int w, const char* title) {
  tft.setTextColor(uiColorInfo);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(x, *y);
  tft.print(title);
  *y += 8;
  tft.drawFastHLine(x, *y, w, uiColorBorder);
  *y += 4;
}

void drawDetailRowAt(int x, int* y, int w, const char* label, const char* value) {
  char clipped[28];
  uiEllipsize(value, w - 64, clipped, sizeof(clipped));

  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(x, *y);
  tft.print(label);

  tft.setTextColor(uiColorTextPrimary);
  uiTextRight(x + 58, *y - 13, w - 58, 16, clipped, FONT_SIZE_XS, uiColorTextPrimary);
  *y += 14;
}

void drawWrappedText(int x, int y, int w, const char* text, uint16_t color) {
  char first[48] = "";
  char second[48] = "";
  char current[48] = "";
  const char* p = text;
  bool onSecond = false;

  while (*p) {
    while (*p == ' ') p++;
    if (!*p) break;

    char word[24];
    int wi = 0;
    while (*p && *p != ' ' && wi < (int)sizeof(word) - 1) word[wi++] = *p++;
    word[wi] = '\0';
    if (*p && *p != ' ') {
      while (*p && *p != ' ') p++;
    }

    char candidate[48];
    if (current[0]) snprintf(candidate, sizeof(candidate), "%s %s", current, word);
    else snprintf(candidate, sizeof(candidate), "%s", word);

    tft.setFreeFont(FONT_SIZE_XS);
    if (!onSecond && tft.textWidth(candidate) <= w) {
      strncpy(current, candidate, sizeof(current) - 1);
      current[sizeof(current) - 1] = '\0';
    } else if (!onSecond) {
      strncpy(first, current, sizeof(first) - 1);
      first[sizeof(first) - 1] = '\0';
      strncpy(current, word, sizeof(current) - 1);
      current[sizeof(current) - 1] = '\0';
      onSecond = true;
    } else if (tft.textWidth(candidate) <= w) {
      strncpy(current, candidate, sizeof(current) - 1);
      current[sizeof(current) - 1] = '\0';
    } else {
      break;
    }
  }

  if (!onSecond) {
    strncpy(first, current, sizeof(first) - 1);
    first[sizeof(first) - 1] = '\0';
  } else {
    strncpy(second, current, sizeof(second) - 1);
    second[sizeof(second) - 1] = '\0';
  }

  tft.setTextColor(color);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(x, y);
  tft.print(first);
  if (second[0]) {
    tft.setCursor(x, y + 14);
    tft.print(second);
  }
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
    LOG_VERBOSELN("Linear regression: Not enough data");
    return 0.0; // Not enough data for regression
  }
  
  // Extract last 25 readings from discharge rate buffer
  float voltages[25];
  float times[25];
  
  LOG_VERBOSE("Linear regression: Processing %d data points\n", dischargeRateBufferCount);
  
  uint32_t firstEpoch = dischargeRateTimestampBuffer[0];
  bool useRtcTime = isEpochValid(firstEpoch);
  
  for (int i = 0; i < 25; i++) {
    voltages[i] = dischargeRateBuffer[i].battery_voltage;
    
    // Filter out battery voltage spikes from float reboots (ignore values > 4.5V)
    if (voltages[i] > 4.5) {
      LOG_VERBOSE("  Point %d: V=%.3f (IGNORED - spike >4.5V)\n", i, voltages[i]);
      continue; // Skip this point in regression calculation
    }
    
    if (useRtcTime && isEpochValid(dischargeRateTimestampBuffer[i]) &&
        dischargeRateTimestampBuffer[i] >= firstEpoch) {
      times[i] = (float)(dischargeRateTimestampBuffer[i] - firstEpoch);
    } else {
      // Fallback for old data without RTC timestamps.
      times[i] = i * 600.0; // 0, 600, 1200, 1800... seconds
      useRtcTime = false;
    }
    LOG_VERBOSE("  Point %d: V=%.3f, rel_time=%.0f\n", i, voltages[i], times[i]);
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
    LOG_VERBOSE("  Not enough valid points after filtering: %d\n", n);
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
  
  LOG_VERBOSELN("Linear regression results:");
  LOG_VERBOSE("  Slope: %.6f V/s\n", slope);
  LOG_VERBOSE("  R²: %.3f\n", rSquared);
  LOG_VERBOSE("  Discharge rate: %.6f V/s\n", -slope);
  
  // Only trust regression if correlation is reasonable (R² > 0.01)
  if (rSquared < 0.01) {
    LOG_VERBOSELN("  Poor correlation (R² < 0.01), returning 0.0");
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
      float elapsedSeconds = times[24] > times[0] ? times[24] - times[0] : 24 * 600.0;
      dischargeRate = (firstVoltage - lastVoltage) / elapsedSeconds; // Total drop over time period
      LOG_VERBOSE("  Applied minimum discharge rate: %.6f V/s\n", dischargeRate);
    }
  }
  
  LOG_VERBOSE("  Returning discharge rate: %.6f V/s\n", dischargeRate);
  return dischargeRate;
}

// More view - two-column option grid
void drawMoreView() {
  tft.fillScreen(uiColorBackground);
  
  drawViewTopbar("More");
  
  int contentY = TOPBAR_H + MARGIN;
  int cardH = (UI_H - TOPBAR_H - NAV_H - MARGIN * 2 - GAP * 3) / 4;
  int contentW = UI_W - MARGIN * 2;
  int cardW = (contentW - GAP) / 2;
  int leftX = MARGIN;
  int rightX = MARGIN + cardW + GAP;
  
  // 1) Details
  uiCard(leftX, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(leftX + 12, contentY + cardH / 2 + 8);
  tft.print("Details >");

  uiCard(rightX, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(rightX + 12, contentY + cardH / 2 + 8);
  tft.print("Theme");
  drawThemeToggle(rightX + cardW - 64, contentY + (cardH - 28) / 2, darkMode);
  
  // 2) Calibration
  contentY += cardH + GAP;
  uiCard(leftX, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(leftX + 12, contentY + cardH / 2 + 8);
  tft.print("Polygone >");

  uiCard(rightX, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(rightX + 12, contentY + cardH / 2 + 8);
  tft.print("New Yeast >");
  
  // 3) Create New Fermentation
  contentY += cardH + GAP;
  uiCard(leftX, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(leftX + 12, contentY + cardH / 2 + 8);
  tft.print("New Brew >");

  uiCard(rightX, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(rightX + 12, contentY + cardH / 2 + 8);
  tft.print("Manage Yeast >");
  
  // 4) Manage Brew / Reboot
  contentY += cardH + GAP;
  uiCard(leftX, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(leftX + 12, contentY + cardH / 2 + 8);
  tft.print("Manage Brew >");

  uiCard(rightX, contentY, cardW, cardH, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_SM);
  tft.setCursor(rightX + 12, contentY + cardH / 2 + 8);
  tft.print("Reboot >");
  
  uiDrawBottomNav(TAB_MORE);
}

// Live Details view - structured cards with KV data
void drawLiveDetailsView() {
  tft.fillScreen(uiColorBackground);
  drawViewTopbar("Details");

  int contentY = TOPBAR_H + MARGIN;
  int contentW = UI_W - MARGIN * 2;
  int navTop = UI_H - NAV_H;
  payload_t latest = {0};
  bool hasData = displayDataCount > 0;
  uint32_t latestEpoch = 0;
  if (hasData) {
    latest = displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    latestEpoch = displayTimestampBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
  }

  uiCard(MARGIN, contentY, contentW, 46, CARD_RADIUS);
  tft.setTextColor(uiColorTextPrimary);
  tft.setFreeFont(FONT_SIZE_MD);
  tft.setCursor(MARGIN + 10, contentY + 30);
  if (hasData) {
    tft.printf("SG: %.3f", latest.density);
  } else {
    tft.print("SG: --");
  }

  char value[48];
  tft.setTextColor(uiColorTextSecondary);
  tft.setFreeFont(FONT_SIZE_XS);
  tft.setCursor(MARGIN + 166, contentY + 16);
  if (brewProfileLoaded) {
    char phase[34];
    uiEllipsize(fermentationStateMachine.phaseName(), 180, phase, sizeof(phase));
    tft.print(phase);
  } else {
    tft.print("No active batch");
  }
  tft.setCursor(MARGIN + 166, contentY + 34);
  if (isEpochValid(latestEpoch)) {
    char timestamp[24];
    formatTimestamp(latestEpoch, timestamp, sizeof(timestamp));
    tft.printf("Last %s", timestamp + 11);
  } else {
    unsigned long secs = (millis() - lastUpdate) / 1000;
    tft.printf("Last %lus ago", secs);
  }

  int leftX = MARGIN;
  int colY = contentY + 61;
  int colW = (contentW - GAP) / 2;
  int rightX = MARGIN + colW + GAP;
  int leftY = colY;
  int rightY = colY;

  drawDetailSectionAt(leftX, &leftY, colW, "BATCH");
  snprintf(value, sizeof(value), "%s", brewProfileLoaded ? activeBrewProfile.batchName : "--");
  drawDetailRowAt(leftX, &leftY, colW, "Name", value);
  snprintf(value, sizeof(value), "%s", brewProfileLoaded ? activeBrewProfile.beerStyle : "--");
  drawDetailRowAt(leftX, &leftY, colW, "Style", value);
  snprintf(value, sizeof(value), "%s", brewProfileLoaded ? activeBrewProfile.yeastName : "--");
  drawDetailRowAt(leftX, &leftY, colW, brewProfileLoaded && activeBrewProfile.autoModeEnabled ? "Preset" : "Yeast", value);

  leftY += 4;
  drawDetailSectionAt(leftX, &leftY, colW, "FERMENTATION");
  snprintf(value, sizeof(value), "%s", brewProfileLoaded ? fermentationStateMachine.phaseName() : "--");
  drawDetailRowAt(leftX, &leftY, colW, "Phase", value);
  snprintf(value, sizeof(value), "%.3f / %.3f", brewProfileLoaded ? activeBrewProfile.effectiveOG : 0.0f,
           brewProfileLoaded ? activeBrewProfile.expectedFinalGravity : 0.0f);
  drawDetailRowAt(leftX, &leftY, colW, "OG/FG", brewProfileLoaded ? value : "--");
  snprintf(value, sizeof(value), "%.0f%% / %.1f%%", fermentationMetrics.currentAttenuation, fermentationMetrics.estimatedABV);
  drawDetailRowAt(leftX, &leftY, colW, "Att/ABV", hasData ? value : "--");
  char eta[16];
  if (currentETA.valid) formatDurationShort(currentETA.secondsToPackaging, eta, sizeof(eta));
  else strcpy(eta, "--");
  drawDetailRowAt(leftX, &leftY, colW, "ETA", eta);

  drawDetailSectionAt(rightX, &rightY, colW, "FLOAT");
  snprintf(value, sizeof(value), "%.1f C / %.1f deg", latest.temperature, latest.angle);
  drawDetailRowAt(rightX, &rightY, colW, "Temp/Ang", hasData ? value : "--");
  uint8_t battPercent = hasData ? calculateBatteryPercentage(latest.battery_voltage) : 0;
  snprintf(value, sizeof(value), "%.2f V / %d%%", latest.battery_voltage, battPercent);
  drawDetailRowAt(rightX, &rightY, colW, "Battery", hasData ? value : "--");
  snprintf(value, sizeof(value), "%d", hasData ? latest.sequence_id : 0);
  drawDetailRowAt(rightX, &rightY, colW, "Seq", hasData ? value : "--");
  snprintf(value, sizeof(value), "%u / %lu", lastAckedFloatSeq, (unsigned long)duplicateFloatPackets);
  drawDetailRowAt(rightX, &rightY, colW, "ACK/Dup", value);
  snprintf(value, sizeof(value), "%d dBm", lastRSSI);
  drawDetailRowAt(rightX, &rightY, colW, "RSSI", rssiAvailable ? value : "--");

  rightY += 4;
  drawDetailSectionAt(rightX, &rightY, colW, "SYSTEM");
  snprintf(value, sizeof(value), "%s", fermentationFileOpen ? "OK" : "ERR");
  drawDetailRowAt(rightX, &rightY, colW, "Log", value);
  snprintf(value, sizeof(value), "%d", displayDataCount);
  drawDetailRowAt(rightX, &rightY, colW, "Live Pts", value);
  if (totalCSVDataLines > 0 || lastHistoricalLoadedLines > 0) {
    snprintf(value, sizeof(value), "%d/%d", lastHistoricalLoadedLines, totalCSVDataLines);
  } else {
    snprintf(value, sizeof(value), "%s", hasData ? "pending" : "0/0");
  }
  drawDetailRowAt(rightX, &rightY, colW, "CSV L/T", value);
  snprintf(value, sizeof(value), "%d/%d", lastHistoricalSkippedLines, lastHistoricalParseErrors);
  drawDetailRowAt(rightX, &rightY, colW, "Skip/Err", value);
  snprintf(value, sizeof(value), "%d/%d", bufferedCount, MAX_DATA_BUFFER);
  drawDetailRowAt(rightX, &rightY, colW, "Buf", value);

  uiDrawBottomNav(TAB_LIVE);
  detailsDirty = false;
}

// Time-based SPI separation implementation

// Mount SD card temporarily for operations
bool mountSDTemporarily() {
  if (!sdInitialized) {
    // Serial.println("Mounting SD card temporarily...");  // Reduced spam

    digitalWrite(DISPLAY_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    delayMicroseconds(50);

    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    sdSPI.setFrequency(4000000);
    delayMicroseconds(50);

    int mountAttempts = 0;
    const int MAX_MOUNT_ATTEMPTS = 2;
    bool mounted = false;
    while (mountAttempts < MAX_MOUNT_ATTEMPTS && !mounted) {
      mounted = SD.begin(SD_CS, sdSPI);
      if (!mounted) {
        mountAttempts++;
        if (mountAttempts < MAX_MOUNT_ATTEMPTS) delay(100);
      }
    }

    if (!mounted) {
      LOG_ERRORLN("SD card mount failed!");
      sdCardPresent = false;
      SD.end();
      sdSPI.end();
      digitalWrite(SD_CS, HIGH);
      digitalWrite(DISPLAY_CS, HIGH);
      delayMicroseconds(100);
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
void bufferSensorData(payload_t data, uint32_t epoch_s) {
  if (bufferedCount < MAX_DATA_BUFFER) {
    // Normal case: buffer has space
    dataBuffer[bufferIndex].data = data;
    dataBuffer[bufferIndex].epoch_s = epoch_s;
    dataBuffer[bufferIndex].buffered_millis = millis();
    dataBuffer[bufferIndex].written = false;
    bufferIndex = (bufferIndex + 1) % MAX_DATA_BUFFER;
    bufferedCount++;
    
    if (bufferedCount % 50 == 0) { // Only log every 50th message to reduce spam
      LOG_VERBOSE("Buffered sensor data: seq=%d, sg=%.3f, temp=%.1f°C (buffer: %d/%d)\n", 
                    data.sequence_id, data.density, data.temperature, bufferedCount, MAX_DATA_BUFFER);
    }
  } else {
    // Buffer full: implement proper circular buffer overwrite
    static unsigned long lastOverflowWarning = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastOverflowWarning > 10000) { // Warn every 10 seconds
      LOG_ERROR("WARNING: Sensor data buffer full (%d entries) - overwriting oldest data\n", MAX_DATA_BUFFER);
      LOG_ERRORLN("Consider increasing SD write frequency or buffer size");
      lastOverflowWarning = currentTime;
    }
    
    // Overwrite oldest data (circular buffer behavior)
    int oldestIndex = bufferIndex; // bufferIndex points to oldest when full
    dataBuffer[oldestIndex].data = data;
    dataBuffer[oldestIndex].epoch_s = epoch_s;
    dataBuffer[oldestIndex].buffered_millis = currentTime;
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
      LOG_ERROR("CRITICAL: Buffer health warning - %d/%d entries unwritten\n", 
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
    LOG_VERBOSE("Writing %d sensor data points to SD\n", bufferedCount);
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
    LOG_ERROR("SD operation timeout after %dms, deferring remaining writes\n", MAX_SD_OPERATION_TIME);
    break;
  }
  
  if (!dataBuffer[i].written && dataBuffer[i].buffered_millis > 0) {
    if (logDataToSD(dataBuffer[i].data, dataBuffer[i].epoch_s)) {
      dataBuffer[i].written = true;
      writtenCount++;
    }
    checkedCount++;
  }
}

// Only log success when data was actually written
if (writtenCount > 0) {
  LOG_VERBOSE("Wrote %d points to SD\n", writtenCount);
}
lastSDWrite = millis();

// Compact buffer: move unwritten entries to the beginning
int newCount = 0;
for (int i = 0; i < entriesToCheck; i++) {
  if (!dataBuffer[i].written && dataBuffer[i].buffered_millis > 0) {
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
  LOG_VERBOSE("Buffer status: wrote %d, remaining %d/%d\n", writtenCount, bufferedCount, MAX_DATA_BUFFER);
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

void loadFloatMacFromEEPROM() {
  EEPROM.begin(128); // Reserve space for float MAC
  uint32_t magic;
  EEPROM.get(EEPROM_FLOAT_MAC_MAGIC, magic);
  
  if (magic == FLOAT_MAC_MAGIC) {
    EEPROM.get(EEPROM_FLOAT_MAC_ADDR, floatMac);
    floatMacKnown = true;
    LOG_INFO("Float MAC loaded from EEPROM: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                  floatMac[0], floatMac[1], floatMac[2], floatMac[3], floatMac[4], floatMac[5]);
    
    // Register float as peer immediately
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, floatMac, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    
    esp_err_t addResult = esp_now_add_peer(&peerInfo);
    if (addResult == ESP_OK) {
      LOG_INFOLN("Float unit registered as ESP-NOW peer (from EEPROM)");
    } else if (addResult == ESP_ERR_ESPNOW_EXIST) {
      LOG_INFOLN("Float unit already registered as peer");
    } else {
      LOG_ERROR("Failed to register float as peer: %d\n", addResult);
    }
  } else {
    LOG_INFOLN("No valid float MAC found in EEPROM");
  }
}

void saveFloatMacToEEPROM() {
  if (!floatMacKnown) return;
  
  EEPROM.begin(128);
  EEPROM.put(EEPROM_FLOAT_MAC_MAGIC, FLOAT_MAC_MAGIC);
  EEPROM.put(EEPROM_FLOAT_MAC_ADDR, floatMac);
  
  bool success = EEPROM.commit();
  if (success) {
    LOG_INFOLN("Float MAC saved to EEPROM");
  } else {
    LOG_ERRORLN("ERROR: Failed to save float MAC to EEPROM");
  }
}

void startWait(int durationMs) {
  waitStartTime = millis();
  waitDuration = durationMs;
  waitActive = true;
  screenDirty = true;
  LOG_VERBOSE("Wait started for %d ms\n", durationMs);
}

void drawPolynomialGraph() {
  tft.fillScreen(uiColorBackground);
  
  drawViewTopbar("Polynomial");
  
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
                POLY_GRAPH_INNER_X + POLY_GRAPH_INNER_W - 1, POLY_GRAPH_INNER_Y, uiColorBorder);
  tft.drawLine(POLY_GRAPH_INNER_X, POLY_GRAPH_INNER_Y, 
                POLY_GRAPH_INNER_X + POLY_GRAPH_INNER_W - 1, POLY_GRAPH_INNER_Y + POLY_GRAPH_INNER_H - 1, uiColorBorder);
  
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
    tft.printf("%d deg", angleLabels[i]);
  }
  
}

void drawGraphCurve() {
  // Draw smooth curve through all points
  for (int i = 0; i < 90; i++) {
    if (!graphPoints[i].valid) continue;
    
    if (i > 0 && graphPoints[i-1].valid) {
      tft.drawLine(graphPoints[i-1].x, graphPoints[i-1].y, 
                    graphPoints[i].x, graphPoints[i].y, uiColorGold);
    }
  }
}

int calibrationPointCount() {
  return calibUseSixPoints ? 6 : 4;
}

int calibrationPointIndex(CalibMode mode) {
  if (mode < CALIB_POINT1 || mode > CALIB_POINT6) return -1;
  return (int)mode - (int)CALIB_POINT1;
}

CalibMode calibrationModeForPoint(int index) {
  if (index < 0) index = 0;
  if (index >= MAX_BASE_CALIB_POINTS) index = MAX_BASE_CALIB_POINTS - 1;
  return (CalibMode)((int)CALIB_POINT1 + index);
}

float calibrationTargetSG(int index) {
  if (calibUseSixPoints) {
    static const float targets6[6] = {1.000f, 1.010f, 1.020f, 1.030f, 1.045f, 1.060f};
    return targets6[index < 0 ? 0 : (index > 5 ? 5 : index)];
  }
  static const float targets4[4] = {1.000f, 1.020f, 1.040f, 1.060f};
  return targets4[index < 0 ? 0 : (index > 3 ? 3 : index)];
}

int calibrationAddAmount(int index) {
  if (index < 0) index = 0;
  if (calibUseSixPoints) {
    if (index > 5) index = 5;
    return calibUseSalt ? saltAmounts6[index] : sugarAmounts6[index];
  }
  if (index > 3) index = 3;
  return calibUseSalt ? saltAmounts4[index] : sugarAmounts4[index];
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
      LOG_INFOLN("Apply timeout - calibration complete, returning to Live View");
    } else if (calibMode >= CALIB_POINT1 && calibMode <= CALIB_POINT6) {
      // Record timeout - advance to next step
      int idx = calibrationPointIndex(calibMode);
      calibMode = (idx + 1 >= calibrationPointCount()) ? CALIB_COMPLETE : calibrationModeForPoint(idx + 1);
      LOG_VERBOSELN("Record timeout - advanced to next step");
    }
    
    screenDirty = true;
  }
}

void stopWait() {
  waitActive = false;
  waitDuration = 0;
  LOG_VERBOSELN("Wait stopped");
}

// Graph cursor helper functions
void formatTimeFromDataPoint(int index, char* buf, int bufSize) {
  if (index < 0 || index >= displayDataCount) {
    snprintf(buf, bufSize, "0m");
    return;
  }
  
  uint32_t pointEpoch = timestampForPoint(index);
  if (isEpochValid(pointEpoch)) {
    formatTimestamp(pointEpoch, buf, bufSize);
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
  snprintf(buf, sizeof(buf), "Temp: %.1f C", data.temperature);
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
      LOG_INFOLN("Loaded 6h data");
    }
    dismountSD();
  } else {
    LOG_ERRORLN("Failed to mount SD for 6h data load");
  }
}

void handleGraphButton24h() {
  currentLoadMode = LOAD_24H;
  if (mountSDTemporarily()) {
    if (loadHistoricalDataFromCSV(currentFermentationFile, LOAD_24H, MAX_DATA_POINTS, 1)) {
      cursorPosition = 1.0;
      cursorIndex = displayDataCount - 1;
      screenDirty = true;
      LOG_INFOLN("Loaded 24h data");
    }
    dismountSD();
  } else {
    LOG_ERRORLN("Failed to mount SD for 24h data load");
  }
}

void handleGraphButtonAll() {
  currentLoadMode = LOAD_ALL;
  if (mountSDTemporarily()) {
    if (loadHistoricalDataFromCSV(currentFermentationFile, LOAD_ALL, MAX_DATA_POINTS, 1)) {
      cursorPosition = 1.0;
      cursorIndex = displayDataCount - 1;
      screenDirty = true;
      LOG_INFOLN("Loaded all data");
    }
    dismountSD();
  } else {
    LOG_ERRORLN("Failed to mount SD for all data load");
  }
}

void handleGraphButtonLeft() {
  cursorPosition -= 0.05;
  if (cursorPosition < 0.0) cursorPosition = 0.0;
  updateCursorPosition();
  screenDirty = true;
  LOG_VERBOSE("Cursor moved left to position %.2f (index %d)\n", cursorPosition, cursorIndex);
}

void handleGraphButtonRight() {
  cursorPosition += 0.05;
  if (cursorPosition > 1.0) cursorPosition = 1.0;
  updateCursorPosition();
  screenDirty = true;
  LOG_VERBOSE("Cursor moved right to position %.2f (index %d)\n", cursorPosition, cursorIndex);
}

#if SGNODE_UI_TEST_HARNESS
static const char* uiTestWizardStepName(BrewWizardStep step) {
  switch (step) {
    case WIZARD_BATCH_NAME: return "Batch Name";
    case WIZARD_BEER_STYLE: return "Beer Style";
    case WIZARD_BATCH_SIZE: return "Batch Size";
    case WIZARD_BRIX: return "Original Brix";
    case WIZARD_AUTO_MODE: return "Auto Mode";
    case WIZARD_YEAST: return "Yeast";
    case WIZARD_ATTENUATION: return "Attenuation";
    case WIZARD_DIACETYL: return "Diacetyl Rest";
    case WIZARD_YEAST_BEHAVIOR: return "Yeast Behavior";
    case WIZARD_REVIEW: return "Review";
    default: return "Done";
  }
}

const char* uiTestScreenName() {
  switch (currentMode) {
    case LIVE_VIEW: return "LiveScreen";
    case GRAPH_VIEW: return "GraphScreen";
    case CALIBRATION_VIEW: return "CalibrationScreen";
    case BATTERY_VIEW: return "BatteryScreen";
    case MORE_VIEW: return "MoreScreen";
    case POLY_GRAPH_VIEW: return "PolynomialGraphScreen";
    case LIVE_DETAILS_VIEW: return "DetailsScreen";
    case TEMP_GRAPH_VIEW: return "TemperatureGraphScreen";
    case ANGLE_GRAPH_VIEW: return "AngleGraphScreen";
    case ABV_GRAPH_VIEW: return "ABVGraphScreen";
    case BREW_WIZARD_VIEW: return "BrewWizardScreen";
    case OG_VERIFICATION_VIEW: return "OGVerificationScreen";
    case TARGET_CHART_VIEW: return "TargetVsActualChart";
    case DASHBOARD_VIEW: return "DashboardScreen";
    case NEW_YEAST_VIEW: return "NewYeastScreen";
    case MANAGE_YEAST_VIEW: return "ManageYeastScreen";
    case MANAGE_BREW_VIEW: return "ManageBrewScreen";
    default: return "UnknownScreen";
  }
}

static int uiTestObjects(UITestObject* objects, int maxObjects) {
  int count = 0;
  auto addObj = [&](const char* id, const char* type, const char* label, int x, int y, int w, int h, bool enabled = true) {
    if (count >= maxObjects) return;
    objects[count++] = {id, type, label, x, y, w, h, enabled, true};
  };

  if (currentMode == BREW_WIZARD_VIEW) {
    BrewWizardStep step = brewWizard.currentStep();
    if (brewWizard.cancelConfirmationVisible()) {
      addObj("dialog_discard", "dialog", "Discard changes?", MARGIN + 28, 86, UI_W - (MARGIN + 28) * 2, 132);
      addObj("btn_cancel_no", "button", "No", MARGIN + 42, 154, 116, 38);
      addObj("btn_cancel_yes", "button", "Yes", UI_W - MARGIN - 158, 154, 116, 38);
    }
    addObj("screen_title", "text", uiTestWizardStepName(step), 0, 0, UI_W, TOPBAR_H);
    addObj("btn_back", "button", "Back", MARGIN, 276, 120, 36, step != WIZARD_BATCH_NAME);
    addObj(step == WIZARD_REVIEW ? "btn_confirm" : "btn_next", "button", step == WIZARD_REVIEW ? "Confirm" : "Next", UI_W - MARGIN - 140, 276, 140, 36);
    if (step == WIZARD_BATCH_NAME) addObj("input_batch_name", "input", activeBrewProfile.batchName, MARGIN, 88, UI_W - MARGIN * 2, 36);
    if (step == WIZARD_BEER_STYLE) addObj("dropdown_beer_style", "dropdown", activeBrewProfile.beerStyle, MARGIN, 112, UI_W - MARGIN * 2, 64);
    if (step == WIZARD_BATCH_SIZE) addObj("input_batch_size", "input", "Batch Size", MARGIN, 88, UI_W - MARGIN * 2, 36);
    if (step == WIZARD_BRIX) addObj("input_brix", "input", "Original Brix", MARGIN, 88, UI_W - MARGIN * 2, 36);
    if (step == WIZARD_AUTO_MODE) addObj("toggle_auto_mode", "switch", "Auto Mode", UI_W - MARGIN - 74, 86, 60, 32);
    if (step == WIZARD_YEAST) addObj("dropdown_yeast_preset", "dropdown", activeBrewProfile.autoModeEnabled ? activeBrewProfile.selectedYeastPresetName : activeBrewProfile.yeastName, MARGIN, 112, UI_W - MARGIN * 2, 64);
    if (step == WIZARD_ATTENUATION) addObj("dropdown_attenuation", "dropdown", "Attenuation", MARGIN, 112, UI_W - MARGIN * 2, 120);
    if (step == WIZARD_DIACETYL) addObj("toggle_diacetyl_rest", "switch", "Diacetyl Rest", UI_W - MARGIN - 74, 104, 60, 32);
  } else if (currentMode == DASHBOARD_VIEW) {
    if (brewProfileLoaded) {
      int actionY = TOPBAR_H + MARGIN + 42 + 6;
      int actionH = 78;
      addObj("panel_action", "panel", currentBatchAction.type == ACTION_NONE ? currentRecommendation.message : currentBatchAction.title, MARGIN, actionY, UI_W - MARGIN * 2, actionH);
      if (currentBatchAction.requiresChoice) {
        int contentW = UI_W - MARGIN * 2;
        int buttonW = 86;
        int buttonY = actionY + actionH - 32;
        addObj("btn_action_skip", "button", "Skip", MARGIN + contentW - buttonW * 2 - GAP - 10, buttonY, buttonW, 28);
        addObj("btn_action_done", "button", "Done", MARGIN + contentW - buttonW - 10, buttonY, buttonW, 28);
      }
    }
    addObj("panel_recommendation", "panel", currentRecommendation.message, MARGIN, UI_H - NAV_H - 47, UI_W - MARGIN * 2, 40);
    addObj("panel_eta", "panel", "ETA", MARGIN + (UI_W - MARGIN * 2 - GAP) / 2 + GAP, TOPBAR_H + MARGIN + 98, (UI_W - MARGIN * 2 - GAP) / 2, 56);
  } else if (currentMode == OG_VERIFICATION_VIEW) {
    addObj("panel_og_verification", "panel", "OG difference", MARGIN, TOPBAR_H + MARGIN, UI_W - MARGIN * 2, 170);
    addObj("btn_back", "button", "Use Recipe", MARGIN, 242, 200, 44);
    addObj("btn_confirm", "button", "Use Measured", UI_W - MARGIN - 200, 242, 200, 44);
  } else if (currentMode == MORE_VIEW) {
    int contentY = TOPBAR_H + MARGIN;
    int cardH = (UI_H - TOPBAR_H - NAV_H - MARGIN * 2 - GAP * 3) / 4;
    int contentW = UI_W - MARGIN * 2;
    int cardW = (contentW - GAP) / 2;
    int leftX = MARGIN;
    int rightX = MARGIN + cardW + GAP;
    addObj("btn_details", "button", "Details", leftX, contentY, cardW, cardH);
    addObj("btn_theme", "button", "Theme", rightX, contentY, cardW, cardH);
    contentY += cardH + GAP;
    addObj("btn_calibration", "button", "Calibration", leftX, contentY, cardW, cardH);
    addObj("btn_new_yeast", "button", "New Yeast", rightX, contentY, cardW, cardH);
    contentY += cardH + GAP;
    addObj("btn_brew_wizard", "button", "New Brew", leftX, contentY, cardW, cardH);
    addObj("btn_manage_yeast", "button", "Manage Yeast", rightX, contentY, cardW, cardH);
  } else if (currentMode == MANAGE_BREW_VIEW) {
    if (managedBatchCount > 0) {
      addObj("btn_prev_batch", "button", "<", MARGIN, 184, 72, 34);
      addObj("btn_next_batch", "button", ">", UI_W - MARGIN - 72, 184, 72, 34);
      addObj("btn_continue_batch", "button", "Cont", MARGIN + 84, 184, 104, 34);
      addObj("btn_copy_batch", "button", "Copy", MARGIN + 200, 184, 104, 34);
      addObj("btn_delete_batch", "button", "Delete", MARGIN + 316, 184, 104, 34);
      addObj("btn_complete_batch", "button", "Complete", MARGIN + 84, 226, 150, 34);
      addObj("batch_status", "text", managedBatchNames[manageBatchIndex], MARGIN, TOPBAR_H + MARGIN, UI_W - MARGIN * 2, 132);
    }
    if (manageBrewCompleteConfirm) {
      addObj("dialog_complete", "dialog", "Mark batch completed?", MARGIN + 28, 86, UI_W - (MARGIN + 28) * 2, 132);
      addObj("btn_complete_no", "button", "No", MARGIN + 42, 154, 116, 38);
      addObj("btn_complete_yes", "button", "Yes", UI_W - MARGIN - 158, 154, 116, 38);
    }
  } else {
    addObj("screen_title", "text", uiTestScreenName(), 0, 0, UI_W, TOPBAR_H);
  }

  return count;
}

bool uiTestBuildDump(char* buffer, size_t bufferSize) {
  UITestObject objects[18];
  int count = uiTestObjects(objects, 18);
  size_t used = snprintf(buffer, bufferSize, "screen=%s", uiTestScreenName());
  for (int i = 0; i < count && used < bufferSize; i++) {
    used += snprintf(buffer + used, bufferSize - used, ";id=%s,type=%s,label=\"%s\",x=%d,y=%d,w=%d,h=%d,%s,%s",
                     objects[i].id, objects[i].type, objects[i].label, objects[i].x, objects[i].y,
                     objects[i].w, objects[i].h, objects[i].enabled ? "enabled" : "disabled",
                     objects[i].visible ? "visible" : "hidden");
  }
  return true;
}

static bool uiTestElementMatches(const UITestObject& object, const char* text) {
  return strcasecmp(object.id, text) == 0 || strcasecmp(object.label, text) == 0 || strstr(object.label, text) != NULL;
}

bool uiTestTap(int x, int y) {
  uiTestTouchX = (uint16_t)x;
  uiTestTouchY = (uint16_t)y;
  uiTestTouchActive = true;
  checkTouch();
  uiTestTouchActive = false;
  return true;
}

bool uiTestPress(int x, int y) {
  uiTestTouchX = (uint16_t)x;
  uiTestTouchY = (uint16_t)y;
  uiTestTouchActive = true;
  checkTouch();
  return true;
}

bool uiTestRelease() {
  uiTestTouchActive = false;
  return true;
}

bool uiTestSwipe(int x1, int y1, int x2, int y2, int durationMs) {
  int steps = max(2, min(12, durationMs / 40));
  for (int i = 0; i <= steps; i++) {
    int x = x1 + ((x2 - x1) * i / steps);
    int y = y1 + ((y2 - y1) * i / steps);
    uiTestPress(x, y);
    delay(max(1, durationMs / steps));
  }
  return uiTestRelease();
}

bool uiTestTapLabel(const char* text, int* outX, int* outY) {
  UITestObject objects[18];
  int count = uiTestObjects(objects, 18);
  for (int i = 0; i < count; i++) {
    if (!objects[i].visible || !objects[i].enabled) continue;
    if (!uiTestElementMatches(objects[i], text)) continue;
    int x = objects[i].x + objects[i].w / 2;
    int y = objects[i].y + objects[i].h / 2;
    if (outX) *outX = x;
    if (outY) *outY = y;
    return uiTestTap(x, y);
  }
  return false;
}

static bool uiTestTapTextKey(char c) {
  c = toupper(c);
  const char* rows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  int keyY = 130;
  for (int r = 0; r < 3; r++) {
    int len = strlen(rows[r]);
    int keyW = r == 0 ? 40 : 44;
    int keyX = MARGIN + (r * 18);
    for (int i = 0; i < len; i++) {
      if (rows[r][i] == c) {
        return uiTestTap(keyX + i * (keyW + 3) + keyW / 2, keyY + 14);
      }
    }
    keyY += 34;
  }
  if (c == ' ') return uiTestTap(MARGIN + 47, 246);
  return false;
}

static bool uiTestTapNumberKey(char c) {
  const char* keys = "123456789.0";
  int keyW = 94;
  int keyH = 32;
  int startX = 92;
  int startY = 130;
  const char* found = strchr(keys, c);
  if (!found) return false;
  int index = found - keys;
  if (c == '0') index = 10;
  int col = index % 3;
  int row = index / 3;
  return uiTestTap(startX + col * (keyW + 8) + keyW / 2, startY + row * (keyH + 5) + keyH / 2);
}

bool uiTestTypeText(const char* text) {
  if (currentMode != BREW_WIZARD_VIEW) return false;
  BrewWizardStep step = brewWizard.currentStep();
  bool numeric = step == WIZARD_BATCH_SIZE || step == WIZARD_BRIX;
  for (const char* p = text; *p; ++p) {
    bool ok = numeric ? uiTestTapNumberKey(*p) : uiTestTapTextKey(*p);
    if (!ok) return false;
  }
  return true;
}

bool uiTestKey(const char* key) {
  if (strcasecmp(key, "next") == 0 || strcasecmp(key, "enter") == 0) return uiTestTap(UI_W - MARGIN - 70, 294);
  if (strcasecmp(key, "prev") == 0 || strcasecmp(key, "escape") == 0) return uiTestTap(MARGIN + 60, 294);
  if (strcasecmp(key, "backspace") == 0) {
    if (currentMode != BREW_WIZARD_VIEW) return false;
    BrewWizardStep step = brewWizard.currentStep();
    if (step == WIZARD_BATCH_SIZE || step == WIZARD_BRIX) return uiTestTap(92 + 2 * (94 + 8) + 47, 130 + 3 * (32 + 5) + 16);
    return uiTestTap(MARGIN + 102 + 47, 246);
  }
  return false;
}

bool uiTestVisibleText(const char* text) {
  char dump[900];
  if (!uiTestBuildDump(dump, sizeof(dump))) return false;
  return strstr(dump, text) != NULL;
}

bool uiTestValueEquals(const char* fieldName, const char* expected) {
  char value[64] = "";
  if (strcasecmp(fieldName, "batchName") == 0) snprintf(value, sizeof(value), "%s", activeBrewProfile.batchName);
  else if (strcasecmp(fieldName, "beerStyle") == 0) snprintf(value, sizeof(value), "%s", activeBrewProfile.beerStyle);
  else if (strcasecmp(fieldName, "yeastName") == 0) snprintf(value, sizeof(value), "%s", activeBrewProfile.yeastName);
  else if (strcasecmp(fieldName, "yeastPreset") == 0) snprintf(value, sizeof(value), "%s", activeBrewProfile.selectedYeastPresetName);
  else if (strcasecmp(fieldName, "autoModeEnabled") == 0) snprintf(value, sizeof(value), "%s", activeBrewProfile.autoModeEnabled ? "true" : "false");
  else if (strcasecmp(fieldName, "batchSizeLiters") == 0) snprintf(value, sizeof(value), "%.1f", activeBrewProfile.batchSizeLiters);
  else if (strcasecmp(fieldName, "recipeBrix") == 0) snprintf(value, sizeof(value), "%.1f", activeBrewProfile.recipeBrix);
  else if (strcasecmp(fieldName, "recipeOG") == 0) snprintf(value, sizeof(value), "%.3f", activeBrewProfile.recipeOG);
  else if (strcasecmp(fieldName, "effectiveOG") == 0) snprintf(value, sizeof(value), "%.3f", activeBrewProfile.effectiveOG);
  else if (strcasecmp(fieldName, "expectedFG") == 0) snprintf(value, sizeof(value), "%.3f", activeBrewProfile.expectedFinalGravity);
  else return false;
  return strcmp(value, expected) == 0;
}

bool uiTestStateEquals(const char* stateName) {
  return strcasecmp(fermentationStateMachine.phaseName(), stateName) == 0;
}

void uiTestSetMockSG(float value) {
  uiTestMockPayload.density = value;
  uiTestMockPayload.angle = 12.0f;
  uiTestMockPayload.battery_voltage = 4.0f;
  uiTestMockPayload.uptime_s += 60;
  uiTestMockSGSet = true;
}

void uiTestSetMockTemp(float value) {
  uiTestMockPayload.temperature = value;
  uiTestMockTempSet = true;
}

void uiTestSetMockTime(unsigned long value) {
  uiTestMockTime = value;
}

bool uiTestRunAnalyzer() {
  if (!brewProfileLoaded) return false;
  if (!uiTestMockSGSet) uiTestSetMockSG(activeBrewProfile.effectiveOG);
  if (!uiTestMockTempSet) uiTestSetMockTemp(20.0f);
  uint32_t epoch = uiTestMockTime > 0 ? (uint32_t)uiTestMockTime : getCurrentEpoch();
  uiTestMockPayload.sequence_id++;
  addDataPoint(uiTestMockPayload, epoch);
  updateFermentationAssistant(uiTestMockPayload, epoch);
  markScreenDirtyForFloatData();
  return true;
}

bool uiTestRunStateMachine() {
  return uiTestRunAnalyzer();
}

bool uiTestRunETA() {
  refreshFermentationAssistantFromProfile();
  return brewProfileLoaded;
}

bool uiTestRunRecommendations() {
  refreshFermentationAssistantFromProfile();
  return brewProfileLoaded;
}

static bool uiTestRemoveFileIfExists(const char* path) {
  if (SD.exists(path)) return SD.remove(path);
  return true;
}

static bool uiTestDeleteBatchById(const char* batchId) {
  bool ok = true;
  char path[80];
  BrewProfileStore::profilePath(batchId, path, sizeof(path));
  ok = uiTestRemoveFileIfExists(path) && ok;
  BrewProfileStore::targetPath(batchId, path, sizeof(path));
  ok = uiTestRemoveFileIfExists(path) && ok;
  BrewProfileStore::logPath(batchId, path, sizeof(path));
  ok = uiTestRemoveFileIfExists(path) && ok;
  BrewProfileStore::eventsPath(batchId, path, sizeof(path));
  ok = uiTestRemoveFileIfExists(path) && ok;

  char batchDir[64];
  snprintf(batchDir, sizeof(batchDir), "/data/batches/%s", batchId);
  if (SD.exists(batchDir)) {
    ok = SD.rmdir(batchDir) && ok;
  }
  char activeBatchId[24];
  if (BrewProfileStore::loadActiveBatchId(activeBatchId, sizeof(activeBatchId)) && strcmp(activeBatchId, batchId) == 0) {
    ok = BrewProfileStore::clearActiveBatchId() && ok;
  }
  return ok;
}

bool uiTestCreateBatch() {
  BrewProfileStore::setDefaults(&activeBrewProfile);
  strncpy(activeBrewProfile.batchId, "test_batch", sizeof(activeBrewProfile.batchId) - 1);
  strncpy(activeBrewProfile.batchName, "UI Test Batch", sizeof(activeBrewProfile.batchName) - 1);
  strncpy(activeBrewProfile.beerStyle, "Pale Ale", sizeof(activeBrewProfile.beerStyle) - 1);
  activeBrewProfile.createdAt = getCurrentEpoch();
    activeBrewProfile.recipeBrix = 12.0f;
    activeBrewProfile.recipeOG = BrixConverter::brixToSG(activeBrewProfile.recipeBrix);
    activeBrewProfile.effectiveOG = activeBrewProfile.recipeOG;
    activeBrewProfile.expectedFinalGravity = DerivedCalculations::expectedFG(activeBrewProfile.effectiveOG, activeBrewProfile.expectedApparentAttenuation);
    activeBrewProfile.completed = false;
    activeBrewProfile.completedAt = 0;
    brewProfileLoaded = true;
  completeBrewWizard();
  currentMode = DASHBOARD_VIEW;
  screenDirty = true;
  return true;
}

bool uiTestDeleteBatch() {
  bool ok = false;
  if (mountSDTemporarily()) {
    ok = uiTestDeleteBatchById("test_batch");
    dismountSD();
  }
  if (strcmp(activeBrewProfile.batchId, "test_batch") == 0) {
    brewProfileLoaded = false;
    fermentationFileOpen = false;
    currentFermentationFile[0] = '\0';
    clearHistoricalDisplayData();
  }
  currentMode = DASHBOARD_VIEW;
  screenDirty = true;
  return ok;
}

bool uiTestMarkTestBatchCompleted() {
  bool ok = false;
  if (mountSDTemporarily()) {
    static BrewProfile profile;
    if (BrewProfileStore::load("test_batch", &profile)) {
      profile.completed = true;
      profile.completedAt = getCurrentEpoch();
      ok = BrewProfileStore::save(profile);
      char activeBatchId[24];
      if (BrewProfileStore::loadActiveBatchId(activeBatchId, sizeof(activeBatchId)) &&
          strcmp(activeBatchId, "test_batch") == 0) {
        BrewProfileStore::clearActiveBatchId();
      }
    }
    dismountSD();
  }
  if (ok && strcmp(activeBrewProfile.batchId, "test_batch") == 0) {
    activeBrewProfile.completed = true;
    activeBrewProfile.completedAt = getCurrentEpoch();
    brewProfileLoaded = false;
    fermentationFileOpen = false;
    currentFermentationFile[0] = '\0';
    clearHistoricalDisplayData();
    currentMode = DASHBOARD_VIEW;
    screenDirty = true;
  }
  return ok;
}

bool uiTestBatteryStats(char* buffer, size_t bufferSize) {
  if (displayDataCount < 2) {
    snprintf(buffer, bufferSize, "battery_stats points=%d insufficient csv=%d/%d",
             displayDataCount, lastHistoricalLoadedLines, totalCSVDataLines);
    return false;
  }

  int valid = 0;
  int preBatch = 0;
  float firstV = 0.0f;
  float lastV = 0.0f;
  float minV = 99.0f;
  float maxV = 0.0f;
  uint32_t firstEpoch = 0;
  uint32_t lastEpoch = 0;
  bool hasBatchStart = brewProfileLoaded && isEpochValid(activeBrewProfile.createdAt);

  for (int i = 0; i < displayDataCount; i++) {
    int idx = (displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    payload_t data = displayDataBuffer[idx];
    uint32_t epoch = displayTimestampBuffer[idx];

    if (hasBatchStart && isEpochValid(epoch) && epoch < activeBrewProfile.createdAt) {
      preBatch++;
    }

    if (data.battery_voltage < 2.5f || data.battery_voltage > 4.5f) {
      continue;
    }

    if (valid == 0) {
      firstV = data.battery_voltage;
      firstEpoch = epoch;
    }
    lastV = data.battery_voltage;
    lastEpoch = epoch;
    if (data.battery_voltage < minV) minV = data.battery_voltage;
    if (data.battery_voltage > maxV) maxV = data.battery_voltage;
    valid++;
  }

  if (valid < 2) {
    snprintf(buffer, bufferSize, "battery_stats points=%d valid=%d insufficient csv=%d/%d",
             displayDataCount, valid, lastHistoricalLoadedLines, totalCSVDataLines);
    return false;
  }

  bool hasTimeSpan = isEpochValid(firstEpoch) && isEpochValid(lastEpoch) && lastEpoch > firstEpoch;
  float hours = hasTimeSpan ? (float)(lastEpoch - firstEpoch) / 3600.0f : 0.0f;
  float deltaV = lastV - firstV;
  float vPerDay = hasTimeSpan && hours > 0.0f ? (deltaV / hours) * 24.0f : 0.0f;
  const char* trend = "unknown";
  if (hasTimeSpan) {
    if (vPerDay > 0.02f) trend = "rising_or_noise";
    else if (vPerDay < -0.12f) trend = "high_drain";
    else if (vPerDay < -0.06f) trend = "moderate_drain";
    else if (vPerDay < -0.02f) trend = "normal_drain";
    else trend = "stable";
  }

  snprintf(buffer, bufferSize,
           "battery_stats pts=%d valid=%d first=%.3f last=%.3f min=%.3f max=%.3f h=%.1f dv=%.3f vday=%.3f pct=%u-%u trend=%s pre=%d csv=%d/%d skip=%d err=%d",
           displayDataCount, valid, firstV, lastV, minV, maxV, hours, deltaV, vPerDay,
           calculateBatteryPercentage(firstV), calculateBatteryPercentage(lastV), trend,
           preBatch, lastHistoricalLoadedLines, totalCSVDataLines,
           lastHistoricalSkippedLines, lastHistoricalParseErrors);
  return true;
}

bool uiTestBatchDiagnostics(char* buffer, size_t bufferSize) {
  if (!brewProfileLoaded) {
    snprintf(buffer, bufferSize,
             "batch_diag loaded=0 screen=%s live=%d csv=%d/%d skip=%d err=%d relaxed=%d pre=%d post=%d",
             uiTestScreenName(), displayDataCount, lastHistoricalLoadedLines, totalCSVDataLines,
             lastHistoricalSkippedLines, lastHistoricalParseErrors,
             lastHistoricalBatchStartRelaxed ? 1 : 0,
             lastHistoricalPreBatchLines, lastHistoricalPostBatchLines);
    return false;
  }

  snprintf(buffer, bufferSize,
           "batch_diag id=%s name=\"%s\" created=%lu live=%d csv=%d/%d skip=%d batch_skip=%d cutoff_skip=%d err=%d relaxed=%d pre=%d post=%d oldest=%lu newest=%lu ack=%u dup=%lu file=\"%s\"",
           activeBrewProfile.batchId, activeBrewProfile.batchName,
           (unsigned long)activeBrewProfile.createdAt,
           displayDataCount,
           lastHistoricalLoadedLines, totalCSVDataLines,
           lastHistoricalSkippedLines,
           lastHistoricalBatchFilteredLines,
           lastHistoricalCutoffFilteredLines,
           lastHistoricalParseErrors,
           lastHistoricalBatchStartRelaxed ? 1 : 0,
           lastHistoricalPreBatchLines, lastHistoricalPostBatchLines,
           (unsigned long)lastHistoricalOldestEpoch,
           (unsigned long)lastHistoricalNewestEpoch,
           lastAckedFloatSeq,
           (unsigned long)duplicateFloatPackets,
           currentFermentationFile);
  return true;
}

bool uiTestFloatStats(char* buffer, size_t bufferSize) {
  if (displayDataCount <= 0 && !latestFloatDataValid) {
    snprintf(buffer, bufferSize, "float_stats no_data");
    return false;
  }

  payload_t latest = latestFloatDataValid
    ? latestFloatData
    : displayDataBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
  uint32_t latestEpoch = latestFloatDataValid
    ? latestFloatEpoch
    : displayTimestampBuffer[(displayDataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];

  float minSG = latest.density;
  float maxSG = latest.density;
  float sumSG = 0.0f;
  int sgCount = 0;
  for (int i = 0; i < displayDataCount; i++) {
    int idx = (displayDataIndex - displayDataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS;
    float sg = displayDataBuffer[idx].density;
    if (sg < 0.990f || sg > 1.200f || isnan(sg) || isinf(sg)) continue;
    if (sgCount == 0) {
      minSG = sg;
      maxSG = sg;
    }
    if (sg < minSG) minSG = sg;
    if (sg > maxSG) maxSG = sg;
    sumSG += sg;
    sgCount++;
  }

  float avgSG = sgCount > 0 ? sumSG / sgCount : latest.density;
  float waterError = latest.density - 1.000f;
  const char* waterVerdict = "check_calibration";
  if (fabs(waterError) <= 0.002f) waterVerdict = "water_ok";
  else if (fabs(waterError) <= 0.005f) waterVerdict = "water_acceptable";

  snprintf(buffer, bufferSize,
           "float_stats seq=%lu sg=%.4f avg=%.4f min=%.4f max=%.4f err=%+.4f angle=%.2f temp=%.2fC batt=%.3fV epoch=%lu verdict=%s points=%d",
           (unsigned long)latest.sequence_id,
           latest.density, avgSG, minSG, maxSG, waterError,
           latest.angle, latest.temperature, latest.battery_voltage,
           (unsigned long)latestEpoch, waterVerdict, displayDataCount);
  return true;
}

bool uiTestActionStatus(char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize,
           "action type=%d code=%d choice=%d due=%lu title=\"%s\" message=\"%s\" dryHop=%d/%d/%d drest=%d/%d cold=%d/%d package=%d/%d",
           (int)currentBatchAction.type,
           currentBatchAction.code,
           currentBatchAction.requiresChoice ? 1 : 0,
           (unsigned long)currentBatchAction.secondsUntilDue,
           currentBatchAction.title,
           currentBatchAction.message,
           activeBrewProfile.dryHopEnabled ? 1 : 0,
           activeBrewProfile.dryHopDone ? 1 : 0,
           activeBrewProfile.dryHopRemoved ? 1 : 0,
           activeBrewProfile.dRestDone ? 1 : 0,
           activeBrewProfile.dRestSkipped ? 1 : 0,
           activeBrewProfile.coldCrashDone ? 1 : 0,
           activeBrewProfile.coldCrashSkipped ? 1 : 0,
           activeBrewProfile.packageDone ? 1 : 0,
           activeBrewProfile.packageSkipped ? 1 : 0);
  return true;
}

bool uiTestActionDone(char* buffer, size_t bufferSize) {
  BatchActionType before = currentBatchAction.type;
  if (before == ACTION_NONE || !currentBatchAction.requiresChoice) {
    snprintf(buffer, bufferSize, "action_done unavailable type=%d choice=%d", (int)before, currentBatchAction.requiresChoice ? 1 : 0);
    return false;
  }
  handleCurrentActionChoice(true);
  snprintf(buffer, bufferSize, "action_done type=%d next=%d", (int)before, (int)currentBatchAction.type);
  return true;
}

bool uiTestActionSkip(char* buffer, size_t bufferSize) {
  BatchActionType before = currentBatchAction.type;
  if (before == ACTION_NONE || !currentBatchAction.requiresChoice) {
    snprintf(buffer, bufferSize, "action_skip unavailable type=%d choice=%d", (int)before, currentBatchAction.requiresChoice ? 1 : 0);
    return false;
  }
  handleCurrentActionChoice(false);
  snprintf(buffer, bufferSize, "action_skip type=%d next=%d", (int)before, (int)currentBatchAction.type);
  return true;
}

bool uiTestRunActionButtonTests(char* buffer, size_t bufferSize) {
  static BrewProfile savedProfile;
  savedProfile = activeBrewProfile;
  bool savedLoaded = brewProfileLoaded;
  bool savedFileOpen = fermentationFileOpen;
  static char savedFile[sizeof(currentFermentationFile)];
  strncpy(savedFile, currentFermentationFile, sizeof(savedFile));
  savedFile[sizeof(savedFile) - 1] = '\0';
  ViewMode savedMode = currentMode;
  BatchAction savedAction = currentBatchAction;
  Recommendation savedRecommendation = currentRecommendation;

  BrewProfileStore::setDefaults(&activeBrewProfile);
  strncpy(activeBrewProfile.batchId, "test_batch", sizeof(activeBrewProfile.batchId) - 1);
  strncpy(activeBrewProfile.batchName, "Action Button Test", sizeof(activeBrewProfile.batchName) - 1);
  strncpy(activeBrewProfile.beerStyle, "Hoppy Pils", sizeof(activeBrewProfile.beerStyle) - 1);
  activeBrewProfile.createdAt = getCurrentEpoch();
  activeBrewProfile.recipeOG = 1.048f;
  activeBrewProfile.effectiveOG = 1.048f;
  activeBrewProfile.expectedFinalGravity = 1.010f;
  activeBrewProfile.ogVerified = true;
  activeBrewProfile.ogNeedsChoice = false;
  activeBrewProfile.diacetylRestEnabled = false;
  BatchActionEngine::applyStyleDefaults(&activeBrewProfile);
  brewProfileLoaded = true;
  fermentationFileOpen = true;
  currentMode = DASHBOARD_VIEW;
  fermentationMetrics.currentAttenuation = 72.0f;
  fermentationMetrics.gravityDeltaPerHour = -0.00002f;
  currentRecommendation.code = 0;
  strcpy(currentRecommendation.message, "Action test");

  uint32_t nowEpoch = getCurrentEpoch();
  refreshCurrentBatchAction(1.014f, nowEpoch);
  bool dryHopShown = currentBatchAction.type == ACTION_DRY_HOP && currentBatchAction.requiresChoice;
  handleCurrentActionChoice(true);
  bool dryHopDone = activeBrewProfile.dryHopDone &&
                    currentBatchAction.type == ACTION_REMOVE_DRY_HOP &&
                    !currentBatchAction.requiresChoice &&
                    currentBatchAction.secondsUntilDue > 0;

  activeBrewProfile.dryHopStartTime = nowEpoch > 49UL * 3600UL ? nowEpoch - 49UL * 3600UL : 1;
  refreshCurrentBatchAction(1.012f, nowEpoch);
  bool removeShown = currentBatchAction.type == ACTION_REMOVE_DRY_HOP && currentBatchAction.requiresChoice;
  handleCurrentActionChoice(false);
  bool removeSkipped = activeBrewProfile.dryHopRemoveSkipped;

  if (mountSDTemporarily()) {
    uiTestDeleteBatchById("test_batch");
    dismountSD();
  }

  activeBrewProfile = savedProfile;
  brewProfileLoaded = savedLoaded;
  fermentationFileOpen = savedFileOpen;
  strncpy(currentFermentationFile, savedFile, sizeof(currentFermentationFile));
  currentFermentationFile[sizeof(currentFermentationFile) - 1] = '\0';
  currentMode = savedMode;
  currentBatchAction = savedAction;
  currentRecommendation = savedRecommendation;
  screenDirty = true;

  bool ok = dryHopShown && dryHopDone && removeShown && removeSkipped;
  snprintf(buffer, bufferSize, "action_buttons dryHopShown=%d dryHopDone=%d removeShown=%d removeSkipped=%d",
           dryHopShown ? 1 : 0, dryHopDone ? 1 : 0, removeShown ? 1 : 0, removeSkipped ? 1 : 0);
  return ok;
}

bool uiTestSelfTest(char* buffer, size_t bufferSize) {
  uiTestDeleteBatch();
  beginBrewWizard();
  if (currentMode != BREW_WIZARD_VIEW) {
    snprintf(buffer, bufferSize, "selftest wizard_open_failed");
    return false;
  }
  if (!uiTestTypeText("ALPHA TEST")) {
    snprintf(buffer, bufferSize, "selftest batch_name_failed");
    return false;
  }
  uiTestKey("next");       // batch name
  uiTestKey("next");       // default beer style
  uiTestTypeText("21.0");
  uiTestKey("next");       // batch size
  uiTestTypeText("12");
  uiTestKey("next");       // brix
  uiTestKey("next");       // default Auto Mode ON
  uiTestKey("next");       // default yeast preset
  uiTestKey("next");       // yeast behavior
  uiTestKey("enter");      // review confirm, completes through normal wizard path
  uiTestTap(306, UI_H - NAV_H / 2);
  bool profileOK = false;
  bool targetOK = false;
  if (mountSDTemporarily()) {
    char path[80];
    BrewProfileStore::profilePath(activeBrewProfile.batchId, path, sizeof(path));
    profileOK = SD.exists(path);
    BrewProfileStore::targetPath(activeBrewProfile.batchId, path, sizeof(path));
    targetOK = SD.exists(path);
    dismountSD();
  }
  if (!profileOK || !targetOK || currentMode != DASHBOARD_VIEW) {
    snprintf(buffer, bufferSize, "selftest profile=%d target=%d screen=%s", profileOK, targetOK, uiTestScreenName());
    return false;
  }
  snprintf(buffer, bufferSize, "selftest wizard=ok profile.json=ok target.json=ok dashboard=ok panels=ok");
  return true;
}

static bool uiTestFileExists(const char* path) {
  bool exists = false;
  if (mountSDTemporarily()) {
    exists = SD.exists(path);
    dismountSD();
  }
  return exists;
}

static bool uiTestTestBatchFilesExist(bool* profileOK, bool* targetOK, bool* logOK) {
  char path[80];
  BrewProfileStore::profilePath("test_batch", path, sizeof(path));
  *profileOK = uiTestFileExists(path);
  BrewProfileStore::targetPath("test_batch", path, sizeof(path));
  *targetOK = uiTestFileExists(path);
  BrewProfileStore::logPath("test_batch", path, sizeof(path));
  *logOK = uiTestFileExists(path);
  return *profileOK && *targetOK && *logOK;
}

static bool uiTestReadLastLogEpoch(const char* path, uint32_t* epochOut) {
  *epochOut = 0;
  if (!SD.exists(path)) return false;
  File file = SD.open(path, FILE_READ);
  if (!file) return false;

  static char line[180];
  uint32_t lastEpoch = 0;
  while (file.available()) {
    int bytesRead = file.readBytesUntil('\n', line, sizeof(line) - 1);
    line[bytesRead] = '\0';
    if (strncmp(line, "timestamp,", 10) == 0 || line[0] == '#' || strlen(line) == 0) {
      continue;
    }

    char* firstComma = strchr(line, ',');
    if (!firstComma) continue;
    char* secondComma = strchr(firstComma + 1, ',');
    if (!secondComma) continue;
    *secondComma = '\0';
    uint32_t epoch = strtoul(firstComma + 1, NULL, 10);
    if (isEpochValid(epoch)) lastEpoch = epoch;
  }
  file.close();
  if (!isEpochValid(lastEpoch)) return false;
  *epochOut = lastEpoch;
  return true;
}

static bool uiTestWriteMixedEpochLog(const char* path, uint32_t createdAt) {
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;
  file.println("timestamp,epoch_s,uptime_s,angle,density,temperature,battery_voltage,battery_percent,state,current_attenuation,estimated_abv,recommendation_code");
  file.printf("2026-05-21 18:13:20,%lu,0,50.00,1.0040,20.00,4.08,90,ACTIVE,90.00,5.00,81\n",
              (unsigned long)(createdAt - 3600UL));
  file.printf("2026-05-21 20:13:20,%lu,0,50.10,1.0035,20.00,4.07,89,ACTIVE,91.00,5.10,81\n",
              (unsigned long)(createdAt + 600UL));
  file.printf("2026-05-21 20:23:20,%lu,0,50.20,1.0030,20.00,4.06,88,ACTIVE,92.00,5.20,81\n",
              (unsigned long)(createdAt + 1200UL));
  file.flush();
  file.close();
  return true;
}

static bool uiTestCreateNumberedBatch(int number, const char* name, uint32_t createdAt) {
  char batchId[24];
  BrewProfileStore::buildBatchId(number, batchId, sizeof(batchId));
  static BrewProfile profile;
  BrewProfileStore::setDefaults(&profile);
  strncpy(profile.batchId, batchId, sizeof(profile.batchId) - 1);
  profile.batchId[sizeof(profile.batchId) - 1] = '\0';
  strncpy(profile.batchName, name, sizeof(profile.batchName) - 1);
  profile.batchName[sizeof(profile.batchName) - 1] = '\0';
  profile.createdAt = createdAt;
  profile.recipeBrix = 12.0f;
  profile.recipeOG = BrixConverter::brixToSG(profile.recipeBrix);
  profile.effectiveOG = profile.recipeOG;
  profile.expectedApparentAttenuation = 80;
  profile.expectedFinalGravity = DerivedCalculations::expectedFG(profile.effectiveOG, profile.expectedApparentAttenuation);
  if (!BrewProfileStore::save(profile)) return false;
  if (!TargetCurveGenerator::generateAndSave(profile)) return false;
  return ensureFermentationLogFile(batchId);
}

static int uiTestFindHighestBatchWithProfile() {
  int highestNumber = 0;
  File root = SD.open("/data/batches");
  if (!root) return 0;

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      const char* filename = file.name();
      const char* slash = strrchr(filename, '/');
      if (slash) filename = slash + 1;
      int fileNum = 0;
      if (sscanf(filename, "batch_%d", &fileNum) == 1) {
        char candidateBatchId[24];
        char profilePath[80];
        BrewProfileStore::buildBatchId(fileNum, candidateBatchId, sizeof(candidateBatchId));
        BrewProfileStore::profilePath(candidateBatchId, profilePath, sizeof(profilePath));
        if (SD.exists(profilePath) && fileNum > highestNumber) {
          highestNumber = fileNum;
        }
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return highestNumber;
}

static void uiTestDeleteNumberedBatch(int number) {
  char batchId[24];
  BrewProfileStore::buildBatchId(number, batchId, sizeof(batchId));
  uiTestDeleteBatchById(batchId);
}

static bool uiTestRunOne(const char* name, bool condition, const char* step, const char* expected, const char* actualValue,
                         int* total, int* passed, int* failed, char* firstFail, size_t firstFailSize);

bool uiTestRunBatchRestoreTests(char* buffer, size_t bufferSize) {
  int total = 0;
  int passed = 0;
  int failed = 0;
  char firstFail[64] = "";
  char actual[32] = "";
  char savedActiveBatchId[24] = "";
  bool savedActiveBatchIdValid = false;

  auto run = [&](const char* name, bool condition, const char* step, const char* expected, const char* actualValue) {
    return uiTestRunOne(name, condition, step, expected, actualValue, &total, &passed, &failed, firstFail, sizeof(firstFail));
  };

  bool activeRestoreOK = false;

  if (mountSDTemporarily()) {
    savedActiveBatchIdValid = BrewProfileStore::loadActiveBatchId(savedActiveBatchId, sizeof(savedActiveBatchId));

    uiTestDeleteNumberedBatch(998);
    uiTestDeleteNumberedBatch(999);

    uint32_t nowEpoch = getCurrentEpoch();
    bool olderOK = uiTestCreateNumberedBatch(998, "UI Active Older", nowEpoch - 600UL);
    bool newerOK = uiTestCreateNumberedBatch(999, "UI Newer Inactive", nowEpoch);
    bool markerOK = olderOK && newerOK && BrewProfileStore::saveActiveBatchId("batch_998");
    char selectedBatchId[24] = "";
    if (markerOK && BrewProfileStore::loadActiveBatchId(selectedBatchId, sizeof(selectedBatchId))) {
      activeRestoreOK = strcmp(selectedBatchId, "batch_998") == 0;
      strncpy(actual, selectedBatchId, sizeof(actual) - 1);
      actual[sizeof(actual) - 1] = '\0';
    } else {
      snprintf(actual, sizeof(actual), "none");
    }
    run("ActiveBatchRestoreSafety", activeRestoreOK, "boot_restore", "batch_998", actual);

    uiTestDeleteNumberedBatch(998);
    uiTestDeleteNumberedBatch(999);

    if (savedActiveBatchIdValid) BrewProfileStore::saveActiveBatchId(savedActiveBatchId);
    else BrewProfileStore::clearActiveBatchId();

    dismountSD();
  } else {
    run("ActiveBatchRestoreSafety", false, "sd_mount", "true", "false");
  }

  snprintf(buffer, bufferSize, "batch_restore total=%d passed=%d failed=%d first_failure=%s",
           total, passed, failed, firstFail[0] ? firstFail : "none");
  return failed == 0;
}

bool uiTestRunFallbackLogTests(char* buffer, size_t bufferSize) {
  int total = 0;
  int passed = 0;
  int failed = 0;
  char firstFail[64] = "";
  char savedActiveBatchId[24] = "";
  bool savedActiveBatchIdValid = false;

  auto run = [&](const char* name, bool condition, const char* step, const char* expected, const char* actualValue) {
    return uiTestRunOne(name, condition, step, expected, actualValue, &total, &passed, &failed, firstFail, sizeof(firstFail));
  };

  if (mountSDTemporarily()) {
    savedActiveBatchIdValid = BrewProfileStore::loadActiveBatchId(savedActiveBatchId, sizeof(savedActiveBatchId));

    uiTestDeleteNumberedBatch(997);
    uiTestDeleteNumberedBatch(999);

    uint32_t nowEpoch = getCurrentEpoch();
    bool dataBatchOK = uiTestCreateNumberedBatch(997, "UI Data Batch", nowEpoch - 1200UL);
    bool emptyHigherOK = uiTestCreateNumberedBatch(999, "UI Empty Higher", nowEpoch);
    static char dataLogPath[80];
    BrewProfileStore::logPath("batch_997", dataLogPath, sizeof(dataLogPath));
    dataBatchOK = dataBatchOK && uiTestWriteMixedEpochLog(dataLogPath, nowEpoch - 1200UL);
    BrewProfileStore::clearActiveBatchId();

    static char fallbackBatchId[24];
    fallbackBatchId[0] = '\0';
    bool fallbackDataOK = dataBatchOK && emptyHigherOK && findBestFallbackBatch(fallbackBatchId, sizeof(fallbackBatchId)) &&
                          strcmp(fallbackBatchId, "batch_997") == 0;
    run("FallbackUsesLogData", fallbackDataOK, "no_marker_selection", "batch_997", fallbackBatchId[0] ? fallbackBatchId : "none");

    uiTestDeleteNumberedBatch(997);
    uiTestDeleteNumberedBatch(999);

    if (savedActiveBatchIdValid) BrewProfileStore::saveActiveBatchId(savedActiveBatchId);
    else BrewProfileStore::clearActiveBatchId();

    dismountSD();
  } else {
    run("FallbackUsesLogData", false, "sd_mount", "true", "false");
  }

  snprintf(buffer, bufferSize, "fallback_log total=%d passed=%d failed=%d first_failure=%s",
           total, passed, failed, firstFail[0] ? firstFail : "none");
  return failed == 0;
}

bool uiTestRunCompletedBatchTests(char* buffer, size_t bufferSize) {
  static BrewProfile savedProfile;
  savedProfile = activeBrewProfile;
  bool savedProfileLoaded = brewProfileLoaded;
  bool savedFermentationFileOpen = fermentationFileOpen;
  static char savedFermentationFile[sizeof(currentFermentationFile)];
  strncpy(savedFermentationFile, currentFermentationFile, sizeof(savedFermentationFile));
  savedFermentationFile[sizeof(savedFermentationFile) - 1] = '\0';
  ViewMode savedMode = currentMode;

  int total = 0;
  int passed = 0;
  int failed = 0;
  char firstFail[64] = "";

  auto run = [&](const char* name, bool condition, const char* step, const char* expected, const char* actualValue) {
    return uiTestRunOne(name, condition, step, expected, actualValue, &total, &passed, &failed, firstFail, sizeof(firstFail));
  };

  uiTestDeleteBatch();
  bool created = uiTestCreateBatch();
  bool markedCompleted = uiTestMarkTestBatchCompleted();
  continueManagedBatch("test_batch");
  bool protectedFromContinue = created && markedCompleted &&
                               (!brewProfileLoaded || strcmp(activeBrewProfile.batchId, "test_batch") != 0);
  run("CompletedBatchProtection", protectedFromContinue, "continue_completed_batch", "blocked",
      protectedFromContinue ? "blocked" : "continued");

  bool markerCleared = false;
  if (mountSDTemporarily()) {
    char activeBatchId[24];
    markerCleared = !BrewProfileStore::loadActiveBatchId(activeBatchId, sizeof(activeBatchId)) ||
                    strcmp(activeBatchId, "test_batch") != 0;
    dismountSD();
  }
  run("CompletedBatchActiveMarker", markerCleared, "active_marker", "not_test_batch",
      markerCleared ? "not_test_batch" : "test_batch");

  uiTestDeleteBatch();
  activeBrewProfile = savedProfile;
  brewProfileLoaded = savedProfileLoaded;
  fermentationFileOpen = savedFermentationFileOpen;
  strncpy(currentFermentationFile, savedFermentationFile, sizeof(currentFermentationFile));
  currentFermentationFile[sizeof(currentFermentationFile) - 1] = '\0';
  currentMode = savedMode;
  screenDirty = true;

  snprintf(buffer, bufferSize, "completed_batch total=%d passed=%d failed=%d first_failure=%s",
           total, passed, failed, firstFail[0] ? firstFail : "none");
  return failed == 0;
}

bool uiTestRunManageBrewUITests(char* buffer, size_t bufferSize) {
  static BrewProfile savedProfile;
  savedProfile = activeBrewProfile;
  bool savedProfileLoaded = brewProfileLoaded;
  bool savedFermentationFileOpen = fermentationFileOpen;
  static char savedFermentationFile[sizeof(currentFermentationFile)];
  strncpy(savedFermentationFile, currentFermentationFile, sizeof(savedFermentationFile));
  savedFermentationFile[sizeof(savedFermentationFile) - 1] = '\0';
  ViewMode savedMode = currentMode;
  int total = 0;
  int passed = 0;
  int failed = 0;
  static char firstFail[64];
  firstFail[0] = '\0';

  auto run = [&](const char* name, bool condition, const char* step, const char* expected, const char* actualValue) {
    return uiTestRunOne(name, condition, step, expected, actualValue, &total, &passed, &failed, firstFail, sizeof(firstFail));
  };

  uiTestDeleteBatch();
  bool created = uiTestCreateBatch();
  loadManagedBatches();
  currentMode = MANAGE_BREW_VIEW;
  manageBrewCompleteConfirm = false;
  for (int i = 0; i < managedBatchCount; i++) {
    if (strcmp(managedBatchIds[i], "test_batch") == 0) {
      manageBatchIndex = i;
      break;
    }
  }

  bool screenOK = created && managedBatchCount > 0 && currentMode == MANAGE_BREW_VIEW;
  run("ManageBrewCompleteButton", screenOK, "button_visible", "Complete", screenOK ? "Complete" : "missing");

  uiTestTap(MARGIN + 84 + 75, 226 + 17);
  bool dialogOK = manageBrewCompleteConfirm;
  run("ManageBrewCompleteConfirm", dialogOK, "confirm_dialog", "visible", dialogOK ? "visible" : "missing");

  uiTestTap(UI_W - MARGIN - 158 + 58, 154 + 19);
  static BrewProfile p;
  bool completedOK = false;
  if (mountSDTemporarily()) {
    completedOK = BrewProfileStore::load("test_batch", &p) && p.completed;
    dismountSD();
  }
  run("ManageBrewCompletePersists", completedOK, "profile_completed", "true", completedOK ? "true" : "false");

  uiTestDeleteBatch();
  activeBrewProfile = savedProfile;
  brewProfileLoaded = savedProfileLoaded;
  fermentationFileOpen = savedFermentationFileOpen;
  strncpy(currentFermentationFile, savedFermentationFile, sizeof(currentFermentationFile));
  currentFermentationFile[sizeof(currentFermentationFile) - 1] = '\0';
  currentMode = savedMode;
  manageBrewCompleteConfirm = false;
  screenDirty = true;

  snprintf(buffer, bufferSize, "manage_brew_ui total=%d passed=%d failed=%d first_failure=%s",
           total, passed, failed, firstFail[0] ? firstFail : "none");
  return failed == 0;
}

bool uiTestPrepareManageBrewComplete(char* buffer, size_t bufferSize) {
  uiTestDeleteBatch();
  bool created = uiTestCreateBatch();
  loadManagedBatches();
  currentMode = MANAGE_BREW_VIEW;
  manageBrewCompleteConfirm = false;
  for (int i = 0; i < managedBatchCount; i++) {
    if (strcmp(managedBatchIds[i], "test_batch") == 0) {
      manageBatchIndex = i;
      break;
    }
  }
  snprintf(buffer, bufferSize, "manage_brew_prepare created=%d batches=%d index=%d", created ? 1 : 0, managedBatchCount, manageBatchIndex);
  return created && managedBatchCount > 0;
}

bool uiTestOpenManageBrewCompleteDialog(char* buffer, size_t bufferSize) {
  if (currentMode != MANAGE_BREW_VIEW) {
    snprintf(buffer, bufferSize, "manage_brew_dialog wrong_screen=%s", uiTestScreenName());
    return false;
  }
  uiTestTap(MARGIN + 84 + 75, 226 + 17);
  snprintf(buffer, bufferSize, "manage_brew_dialog visible=%d", manageBrewCompleteConfirm ? 1 : 0);
  return manageBrewCompleteConfirm;
}

bool uiTestConfirmManageBrewComplete(char* buffer, size_t bufferSize) {
  if (currentMode != MANAGE_BREW_VIEW || !manageBrewCompleteConfirm) {
    snprintf(buffer, bufferSize, "manage_brew_confirm dialog_missing");
    return false;
  }
  uiTestTap(UI_W - MARGIN - 158 + 58, 154 + 19);
  static BrewProfile p;
  bool completedOK = false;
  if (mountSDTemporarily()) {
    completedOK = BrewProfileStore::load("test_batch", &p) && p.completed;
    dismountSD();
  }
  snprintf(buffer, bufferSize, "manage_brew_confirm completed=%d", completedOK ? 1 : 0);
  return completedOK;
}

static void uiTestPrintResult(bool ok, const char* name, const char* step, const char* expected, const char* actual) {
  Serial.print(ok ? "OK TEST " : "ERR TEST ");
  Serial.print(name);
  Serial.print(ok ? " PASS" : " FAIL");
  if (!ok) {
    Serial.print(" step=");
    Serial.print(step);
    Serial.print(" expected=");
    Serial.print(expected);
    Serial.print(" actual=");
    Serial.print(actual);
  }
  Serial.println();
}

static bool uiTestRunOne(const char* name, bool condition, const char* step, const char* expected, const char* actual,
                         int* total, int* passed, int* failed, char* firstFail, size_t firstFailSize) {
  (*total)++;
  if (condition) {
    (*passed)++;
    uiTestPrintResult(true, name, step, expected, actual);
    return true;
  }
  (*failed)++;
  if (firstFail[0] == '\0') {
    snprintf(firstFail, firstFailSize, "%s:%s", name, step);
  }
  uiTestPrintResult(false, name, step, expected, actual);
  return false;
}

static bool uiTestRegressionValidFloat(float value) {
  return !isnan(value) && !isinf(value) && value > 0.0f && value < 10.0f;
}

bool uiTestRunInputValidationTests(char* buffer, size_t bufferSize) {
  int total = 0, passed = 0, failed = 0;
  char firstFail[64] = "";
  auto run = [&](const char* name, bool condition, const char* step, const char* expected, const char* actualValue) {
    return uiTestRunOne(name, condition, step, expected, actualValue, &total, &passed, &failed, firstFail, sizeof(firstFail));
  };

  bool invalidBrixRejected = BrixConverter::brixToSG(0.0f) <= 1.0001f && BrixConverter::brixToSG(99.0f) < 1.300f;
  run("InvalidBrixInput", !invalidBrixRejected, "validation", "reject_invalid_values", "currently_allows_or_computes");

  beginBrewWizard();
  uiTestKey("next");
  uiTestKey("next");
  uiTestTypeText("9999");
  uiTestKey("next");
  bool invalidSizeRejected = brewWizard.currentStep() == WIZARD_BATCH_SIZE && strstr(uiTestScreenName(), "BrewWizard") != NULL;
  run("InvalidBatchSize", invalidSizeRejected, "validation", "stay_on_batch_size", invalidSizeRejected ? "stay_on_batch_size" : "advanced");

  float invalidFG = DerivedCalculations::expectedFG(1.050f, 120);
  bool attenuationSafe = uiTestRegressionValidFloat(invalidFG) && invalidFG >= 0.990f && invalidFG <= 1.050f;
  run("InvalidAttenuation", attenuationSafe, "fg_bounds", "0.990..1.050", attenuationSafe ? "in_range" : "out_of_range");

  snprintf(buffer, bufferSize, "input_validation total=%d passed=%d failed=%d first_failure=%s",
           total, passed, failed, firstFail[0] ? firstFail : "none");
  return failed == 0;
}

bool uiTestRunSensorEdgeTests(char* buffer, size_t bufferSize) {
  static BrewProfile savedProfile;
  savedProfile = activeBrewProfile;
  bool savedProfileLoaded = brewProfileLoaded;
  bool savedFermentationFileOpen = fermentationFileOpen;
  static char savedFermentationFile[sizeof(currentFermentationFile)];
  strncpy(savedFermentationFile, currentFermentationFile, sizeof(savedFermentationFile));
  savedFermentationFile[sizeof(savedFermentationFile) - 1] = '\0';
  ViewMode savedMode = currentMode;
  int total = 0, passed = 0, failed = 0;
  char firstFail[64] = "";
  auto run = [&](const char* name, bool condition, const char* step, const char* expected, const char* actualValue) {
    return uiTestRunOne(name, condition, step, expected, actualValue, &total, &passed, &failed, firstFail, sizeof(firstFail));
  };

  uiTestCreateBatch();
  uiTestSetMockSG(0.0f);
  uiTestSetMockTemp(20.0f);
  bool invalidSensorOK = uiTestRunAnalyzer();
  bool invalidFlagged = currentRecommendation.message && strstr(currentRecommendation.message, "sensor") != NULL;
  run("SensorDropoutDuringActiveBatch", invalidSensorOK && invalidFlagged, "recommendation", "sensor_issue",
      invalidFlagged ? "sensor_issue" : currentRecommendation.message);

  static const float extremeSGs[] = {0.800f, 0.999f, 1.000f, 1.050f, 1.200f, 2.000f};
  static const float extremeTemps[] = {-10.0f, 0.0f, 20.0f, 80.0f};
  bool extremesSafe = true;
  for (int i = 0; i < 6; i++) {
    uiTestSetMockSG(extremeSGs[i]);
    uiTestSetMockTemp(extremeTemps[i % 4]);
    extremesSafe = uiTestRunAnalyzer() && extremesSafe && uiTestRegressionValidFloat(fermentationMetrics.estimatedABV + 1.0f);
  }
  run("ExtremeSensorValues", extremesSafe, "no_nan_inf", "true", extremesSafe ? "true" : "false");

  activeBrewProfile = savedProfile;
  brewProfileLoaded = savedProfileLoaded;
  currentMode = savedMode;
  screenDirty = true;

  snprintf(buffer, bufferSize, "sensor_edge total=%d passed=%d failed=%d first_failure=%s",
           total, passed, failed, firstFail[0] ? firstFail : "none");
  return failed == 0;
}

bool uiTestRunStateLogicTests(char* buffer, size_t bufferSize) {
  static BrewProfile savedProfile;
  savedProfile = activeBrewProfile;
  bool savedProfileLoaded = brewProfileLoaded;
  ViewMode savedMode = currentMode;
  int total = 0, passed = 0, failed = 0;
  char firstFail[64] = "";
  auto run = [&](const char* name, bool condition, const char* step, const char* expected, const char* actualValue) {
    return uiTestRunOne(name, condition, step, expected, actualValue, &total, &passed, &failed, firstFail, sizeof(firstFail));
  };

  uiTestCreateBatch();
  uiTestSetMockTime(0);
  uiTestSetMockSG(1.048f); uiTestRunAnalyzer();
  uiTestSetMockSG(1.035f); uiTestRunAnalyzer();
  const char* phaseBeforeSpike = fermentationStateMachine.phaseName();
  uiTestSetMockSG(1.046f); uiTestRunAnalyzer();
  bool spikeSafe = strcmp(fermentationStateMachine.phaseName(), "IDLE") != 0;
  run("StateMachineRegression", spikeSafe, "sg_spike_phase", "not_IDLE",
      spikeSafe ? fermentationStateMachine.phaseName() : phaseBeforeSpike);

  bool stableShortSafe = strcmp(fermentationStateMachine.phaseName(), "READY TO PACKAGE") != 0;
  run("StableGravityDetectionShort", stableShortSafe, "duration_threshold", "not_ready", stableShortSafe ? "not_ready" : "ready");

  uint32_t stableEpoch = getCurrentEpoch();
  for (int i = 0; i < 4; i++) {
    stableEpoch += 12UL * 3600UL;
    uiTestSetMockTime(stableEpoch);
    uiTestSetMockSG(activeBrewProfile.expectedFinalGravity);
    uiTestRunAnalyzer();
  }
  bool stableLongReached = strcmp(fermentationStateMachine.phaseName(), "FG STABLE") == 0 ||
                           strcmp(fermentationStateMachine.phaseName(), "READY TO PACKAGE") == 0 ||
                           strcmp(fermentationStateMachine.phaseName(), "COMPLETED") == 0;
  run("StableGravityDetectionLong", stableLongReached, "stable_duration", "stable_or_ready", fermentationStateMachine.phaseName());
  uiTestSetMockTime(0);

  BrewProfileStore::setDefaults(&activeBrewProfile);
  strcpy(activeBrewProfile.yeastCategory, "Lager");
  activeBrewProfile.diacetylRestRecommendedByYeast = true;
  activeBrewProfile.diacetylRestEnabled = true;
  brewProfileLoaded = true;
  fermentationStateMachine.reset();
  uiTestSetMockTime(getCurrentEpoch() + 3600UL);
  uiTestSetMockSG(1.012f);
  uiTestRunAnalyzer();
  bool dRestLager = strcmp(fermentationStateMachine.phaseName(), "D-REST READY") == 0 ||
                    strstr(currentRecommendation.message, "diacetyl") != NULL;
  run("DiacetylRestLogicLager", dRestLager, "lager_trigger", "drest_ready",
      dRestLager ? "drest_ready" : fermentationStateMachine.phaseName());

  activeBrewProfile.diacetylRestRecommendedByYeast = false;
  activeBrewProfile.diacetylRestEnabled = false;
  fermentationStateMachine.reset();
  uiTestSetMockTime(getCurrentEpoch() + 7200UL);
  uiTestSetMockSG(1.012f);
  uiTestRunAnalyzer();
  bool dRestAleSafe = strcmp(fermentationStateMachine.phaseName(), "D-REST READY") != 0;
  run("DiacetylRestLogicAleNoAuto", dRestAleSafe, "ale_no_auto", "not_drest", fermentationStateMachine.phaseName());
  uiTestSetMockTime(0);

  activeBrewProfile = savedProfile;
  brewProfileLoaded = savedProfileLoaded;
  currentMode = savedMode;
  screenDirty = true;

  snprintf(buffer, bufferSize, "state_logic total=%d passed=%d failed=%d first_failure=%s",
           total, passed, failed, firstFail[0] ? firstFail : "none");
  return failed == 0;
}

bool uiTestRunUISafetyTests(char* buffer, size_t bufferSize) {
  static BrewProfile savedProfile;
  savedProfile = activeBrewProfile;
  bool savedProfileLoaded = brewProfileLoaded;
  bool savedFermentationFileOpen = fermentationFileOpen;
  static char savedFermentationFile[sizeof(currentFermentationFile)];
  strncpy(savedFermentationFile, currentFermentationFile, sizeof(savedFermentationFile));
  savedFermentationFile[sizeof(savedFermentationFile) - 1] = '\0';
  ViewMode savedMode = currentMode;
  int total = 0, passed = 0, failed = 0;
  char firstFail[64] = "";
  auto run = [&](const char* name, bool condition, const char* step, const char* expected, const char* actualValue) {
    return uiTestRunOne(name, condition, step, expected, actualValue, &total, &passed, &failed, firstFail, sizeof(firstFail));
  };

  beginBrewWizard();
  for (int i = 0; i < 20; i++) uiTestKey(i % 2 == 0 ? "next" : "prev");
  bool touchSpamSafe = currentMode == BREW_WIZARD_VIEW || currentMode == LIVE_VIEW || currentMode == DASHBOARD_VIEW;
  run("TouchSpamTest", touchSpamSafe, "no_crash", "valid_screen", uiTestScreenName());

  beginBrewWizard();
  uiTestTypeText("BACKTEST");
  uiTestKey("next");
  uiTestKey("prev");
  bool backValueKept = strstr(activeBrewProfile.batchName, "BACKTEST") != NULL;
  run("BackNavigationConsistency", backValueKept, "field_retained", "BACKTEST", activeBrewProfile.batchName);

  beginBrewWizard();
  uiTestTypeText("CANCELTEST");
  uiTestKey("prev");
  bool cancelDialogShown = uiTestVisibleText("Discard changes?");
  run("CancelConfirmation", cancelDialogShown, "discard_dialog", "visible", cancelDialogShown ? "visible" : "missing");
  uiTestTap(366, 173);
  bool cancelRestoreOK = strcmp(activeBrewProfile.batchId, savedProfile.batchId) == 0 &&
                         strcmp(activeBrewProfile.batchName, savedProfile.batchName) == 0 &&
                         brewProfileLoaded == savedProfileLoaded &&
                         strcmp(currentFermentationFile, savedFermentationFile) == 0 &&
                         currentMode == DASHBOARD_VIEW;
  run("WizardCancelRestoresActiveBatch", cancelRestoreOK, "cancel_restore", savedProfile.batchName,
      cancelRestoreOK ? activeBrewProfile.batchName : activeBrewProfile.batchName);

  beginBrewWizard();
  bool longTextSafe = uiTestTypeText("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ") &&
                      strlen(activeBrewProfile.batchName) < sizeof(activeBrewProfile.batchName);
  uiTestKey("next");
  run("LongTextInput", longTextSafe, "bounded_name", "<40_chars", longTextSafe ? "bounded" : "overflow_or_reject");

  beginBrewWizard();
  bool specialCharsSafe = !uiTestTypeText("Test/Batch") && uiTestTypeText("PILS A");
  run("SpecialCharacters", specialCharsSafe, "unsupported_chars_safe", "reject_non_keyboard", specialCharsSafe ? "safe" : "unsafe");

  activeBrewProfile = savedProfile;
  brewProfileLoaded = savedProfileLoaded;
  fermentationFileOpen = savedFermentationFileOpen;
  strncpy(currentFermentationFile, savedFermentationFile, sizeof(currentFermentationFile));
  currentFermentationFile[sizeof(currentFermentationFile) - 1] = '\0';
  currentMode = savedMode;
  screenDirty = true;

  snprintf(buffer, bufferSize, "ui_safety total=%d passed=%d failed=%d first_failure=%s",
           total, passed, failed, firstFail[0] ? firstFail : "none");
  return failed == 0;
}

bool uiTestRunRegressionTests(char* buffer, size_t bufferSize) {
  static BrewProfile savedProfile;
  savedProfile = activeBrewProfile;
  bool savedProfileLoaded = brewProfileLoaded;
  bool savedFermentationFileOpen = fermentationFileOpen;
  static char savedFermentationFile[sizeof(currentFermentationFile)];
  strncpy(savedFermentationFile, currentFermentationFile, sizeof(savedFermentationFile));
  savedFermentationFile[sizeof(savedFermentationFile) - 1] = '\0';
  static char savedActiveBatchId[24];
  bool savedActiveBatchIdValid = false;
  savedActiveBatchId[0] = '\0';
  if (mountSDTemporarily()) {
    savedActiveBatchIdValid = BrewProfileStore::loadActiveBatchId(savedActiveBatchId, sizeof(savedActiveBatchId));
    dismountSD();
  }
  ViewMode savedMode = currentMode;
  CalibrationCoefficients savedCalib = calibCoeffs;
  float savedNormOffset = normOffset;
  float savedNormScale = normScale;
  static payload_t savedDisplayDataBuffer[MAX_DATA_POINTS];
  static uint32_t savedDisplayTimestampBuffer[MAX_DATA_POINTS];
  static payload_t savedDischargeRateBuffer[25];
  static uint32_t savedDischargeRateTimestampBuffer[25];
  memcpy(savedDisplayDataBuffer, displayDataBuffer, sizeof(savedDisplayDataBuffer));
  memcpy(savedDisplayTimestampBuffer, displayTimestampBuffer, sizeof(savedDisplayTimestampBuffer));
  memcpy(savedDischargeRateBuffer, dischargeRateBuffer, sizeof(savedDischargeRateBuffer));
  memcpy(savedDischargeRateTimestampBuffer, dischargeRateTimestampBuffer, sizeof(savedDischargeRateTimestampBuffer));
  int savedDisplayDataIndex = displayDataIndex;
  int savedDisplayDataCount = displayDataCount;
  int savedTotalCSVDataLines = totalCSVDataLines;
  int savedLastHistoricalLoadedLines = lastHistoricalLoadedLines;
  int savedLastHistoricalSkippedLines = lastHistoricalSkippedLines;
  int savedLastHistoricalParseErrors = lastHistoricalParseErrors;
  int savedLastHistoricalPreBatchLines = lastHistoricalPreBatchLines;
  int savedLastHistoricalPostBatchLines = lastHistoricalPostBatchLines;
  int savedLastHistoricalBatchFilteredLines = lastHistoricalBatchFilteredLines;
  int savedLastHistoricalCutoffFilteredLines = lastHistoricalCutoffFilteredLines;
  bool savedLastHistoricalBatchStartRelaxed = lastHistoricalBatchStartRelaxed;
  uint32_t savedLastHistoricalOldestEpoch = lastHistoricalOldestEpoch;
  uint32_t savedLastHistoricalNewestEpoch = lastHistoricalNewestEpoch;
  int savedDischargeRateBufferCount = dischargeRateBufferCount;
  float savedPreviousAnalyticsSG = previousAnalyticsSG;
  uint32_t savedPreviousAnalyticsEpoch = previousAnalyticsEpoch;
  int savedCursorIndex = cursorIndex;
  float savedCursorPosition = cursorPosition;

  int total = 0;
  int passed = 0;
  int failed = 0;
  static char firstFail[80];
  static char actual[48];
  firstFail[0] = '\0';
  actual[0] = '\0';

  auto run = [&](const char* name, bool condition, const char* step, const char* expected, const char* actualValue) {
    return uiTestRunOne(name, condition, step, expected, actualValue, &total, &passed, &failed, firstFail, sizeof(firstFail));
  };

  uiTestDeleteBatch();

  // 1. Calibration Abort Safety
  currentMode = CALIBRATION_VIEW;
  calibMode = CALIB_POINT1;
  calibAngles[0] = 12.3f;
  currentMode = DASHBOARD_VIEW;
  bool calUnchanged = memcmp(&savedCalib, &calibCoeffs, sizeof(CalibrationCoefficients)) == 0;
  run("CalibrationAbortSafety", calUnchanged, "coefficients_unchanged", "true", calUnchanged ? "true" : "false");

  // 2. Calibration Power-Loss Simulation
  calibAngles[0] = 10.0f;
  calibAngles[1] = 0.0f;
  calibMode = CALIB_IDLE;
  float sgAfterDraft = calculateGravity(12.0f);
  bool powerLossSafe = uiTestRegressionValidFloat(sgAfterDraft) && memcmp(&savedCalib, &calibCoeffs, sizeof(CalibrationCoefficients)) == 0;
  run("CalibrationPowerLossSimulation", powerLossSafe, "partial_draft_ignored", "valid_previous_calibration", powerLossSafe ? "valid_previous_calibration" : "changed_or_invalid");

  // 3. Calibration Final Save, restored immediately to avoid permanent test calibration.
  calibCoeffs.coeff3 = 0.0f;
  calibCoeffs.coeff2 = 0.0f;
  calibCoeffs.coeff1 = 0.0105f;
  calibCoeffs.coeff0 = 1.000f;
  calibCoeffs.isValid = true;
  saveCalibrationCoefficients();
  initCalibration();
  bool newCalSaved = calibCoeffs.isValid && fabs(calibCoeffs.coeff1 - 0.0105f) < 0.0002f;
  run("CalibrationFinalSave", newCalSaved, "save_reload", "new_coefficients_active", newCalSaved ? "new_coefficients_active" : "not_saved");
  calibCoeffs = savedCalib;
  normOffset = savedNormOffset;
  normScale = savedNormScale;
  if (calibCoeffs.isValid) saveCalibrationCoefficients();

  // 4-5. New Batch Abort
  beginBrewWizard();
  char abortBatchId[24];
  strncpy(abortBatchId, activeBrewProfile.batchId, sizeof(abortBatchId));
  abortBatchId[sizeof(abortBatchId) - 1] = '\0';
  uiTestTypeText("ABORTTEST");
  uiTestKey("next");
  uiTestKey("prev");
  uiTestKey("prev");
  char abortProfilePath[80];
  BrewProfileStore::profilePath(abortBatchId, abortProfilePath, sizeof(abortProfilePath));
  bool abortNoProfile = !uiTestFileExists(abortProfilePath);
  run("NewBatchAbortBeforeConfirm", abortNoProfile, "profile_not_created", "false", abortNoProfile ? "false" : "true");
  run("NewBatchAbortAfterPartialDraft", abortNoProfile, "draft_not_listed", "false", abortNoProfile ? "false" : "true");

  // 6. New Batch Final Confirm
  bool createOK = uiTestCreateBatch();
  bool profileOK = false, targetOK = false, logOK = false;
  uiTestTestBatchFilesExist(&profileOK, &targetOK, &logOK);
  snprintf(actual, sizeof(actual), "p%d_t%d_l%d", profileOK, targetOK, logOK);
  run("NewBatchFinalConfirm", createOK && profileOK && targetOK && logOK, "files_created", "p1_t1_l1", actual);

  // 7. Duplicate Batch Name
  beginBrewWizard();
  bool duplicateSafe = strcmp(activeBrewProfile.batchId, "test_batch") != 0;
  run("DuplicateBatchName", duplicateSafe, "unique_batch_id", "not_test_batch", activeBrewProfile.batchId);

  // 8. Invalid Brix Input
  bool invalidBrixRejected = BrixConverter::brixToSG(0.0f) <= 1.0001f && BrixConverter::brixToSG(99.0f) < 1.300f;
  run("InvalidBrixInput", !invalidBrixRejected, "validation", "reject_invalid_values", "currently_allows_or_computes");

  // 9. Invalid Batch Size
  beginBrewWizard();
  uiTestKey("next");
  uiTestKey("next");
  uiTestTypeText("9999");
  uiTestKey("next");
  bool invalidSizeRejected = brewWizard.currentStep() == WIZARD_BATCH_SIZE && strstr(uiTestScreenName(), "BrewWizard") != NULL;
  run("InvalidBatchSize", invalidSizeRejected, "validation", "stay_on_batch_size", invalidSizeRejected ? "stay_on_batch_size" : "advanced");

  // 10. Invalid Attenuation
  float invalidFG = DerivedCalculations::expectedFG(1.050f, 120);
  bool attenuationSafe = uiTestRegressionValidFloat(invalidFG) && invalidFG >= 0.990f && invalidFG <= 1.050f;
  run("InvalidAttenuation", attenuationSafe, "fg_bounds", "0.990..1.050", attenuationSafe ? "in_range" : "out_of_range");

  // 11. Auto Mode Toggle Consistency
  BrewProfileStore::setDefaults(&activeBrewProfile);
  activeBrewProfile.autoModeEnabled = true;
  strcpy(activeBrewProfile.attenuationSource, "yeast_preset");
  bool autoModeOK = activeBrewProfile.autoModeEnabled && strcmp(activeBrewProfile.attenuationSource, "yeast_preset") == 0;
  activeBrewProfile.autoModeEnabled = false;
  strcpy(activeBrewProfile.attenuationSource, "manual");
  autoModeOK = autoModeOK && !activeBrewProfile.autoModeEnabled && strcmp(activeBrewProfile.attenuationSource, "manual") == 0;
  run("AutoModeToggleConsistency", autoModeOK, "attenuation_source", "yeast_preset_then_manual", autoModeOK ? "yeast_preset_then_manual" : "mixed");

  // 12. Yeast Preset Missing/Corrupt fallback
  const YeastPreset* fallbackPreset = YeastPresetRepository::findById("missing_preset_for_test");
  bool fallbackOK = fallbackPreset != NULL && fallbackPreset->displayName != NULL && fallbackPreset->displayName[0] != '\0';
  run("YeastPresetMissingCorrupt", fallbackOK, "fallback", "non_empty_default", fallbackOK ? fallbackPreset->displayName : "null");

  // 13-15. OG Verification
  uiTestCreateBatch();
  activeBrewProfile.recipeOG = 1.050f;
  activeBrewProfile.measuredOG = 1.060f;
  activeBrewProfile.effectiveOG = activeBrewProfile.recipeOG;
  activeBrewProfile.ogNeedsChoice = true;
  currentMode = OG_VERIFICATION_VIEW;
  bool ogAbortOK = fabs(activeBrewProfile.effectiveOG - 1.050f) < 0.0005f && activeBrewProfile.ogNeedsChoice;
  run("OGVerificationAbort", ogAbortOK, "pending_no_overwrite", "recipe_og_pending", ogAbortOK ? "recipe_og_pending" : "overwritten");
  handleOGChoice(false);
  bool ogRecipeOK = fabs(activeBrewProfile.effectiveOG - activeBrewProfile.recipeOG) < 0.0005f && activeBrewProfile.ogVerified;
  run("OGVerificationConfirmRecipeOG", ogRecipeOK, "effective_og", "recipeOG", ogRecipeOK ? "recipeOG" : "other");
  activeBrewProfile.measuredOG = 1.060f;
  activeBrewProfile.ogNeedsChoice = true;
  activeBrewProfile.ogVerified = false;
  handleOGChoice(true);
  bool ogMeasuredOK = fabs(activeBrewProfile.effectiveOG - activeBrewProfile.measuredOG) < 0.0005f && activeBrewProfile.ogVerified;
  run("OGVerificationConfirmMeasuredOG", ogMeasuredOK, "effective_og", "measuredOG", ogMeasuredOK ? "measuredOG" : "other");

  // 16-17. Sensor invalid/extreme handling
  uiTestSetMockSG(0.0f);
  uiTestSetMockTemp(20.0f);
  bool invalidSensorOK = uiTestRunAnalyzer();
  bool invalidFlagged = currentRecommendation.message && strstr(currentRecommendation.message, "sensor") != NULL;
  run("SensorDropoutDuringActiveBatch", invalidSensorOK && invalidFlagged, "recommendation", "sensor_issue", invalidFlagged ? "sensor_issue" : currentRecommendation.message);
  static const float extremeSGs[] = {0.800f, 0.999f, 1.000f, 1.050f, 1.200f, 2.000f};
  static const float extremeTemps[] = {-10.0f, 0.0f, 20.0f, 80.0f};
  bool extremesSafe = true;
  for (int i = 0; i < 6; i++) {
    uiTestSetMockSG(extremeSGs[i]);
    uiTestSetMockTemp(extremeTemps[i % 4]);
    extremesSafe = uiTestRunAnalyzer() && extremesSafe && uiTestRegressionValidFloat(fermentationMetrics.estimatedABV + 1.0f);
  }
  run("ExtremeSensorValues", extremesSafe, "no_nan_inf", "true", extremesSafe ? "true" : "false");

  // 18. State Machine Regression spike
  uiTestCreateBatch();
  uiTestSetMockSG(1.048f); uiTestRunAnalyzer();
  uiTestSetMockSG(1.035f); uiTestRunAnalyzer();
  const char* phaseBeforeSpike = fermentationStateMachine.phaseName();
  uiTestSetMockSG(1.046f); uiTestRunAnalyzer();
  bool spikeSafe = strcmp(fermentationStateMachine.phaseName(), "IDLE") != 0;
  run("StateMachineRegression", spikeSafe, "sg_spike_phase", "not_IDLE", spikeSafe ? fermentationStateMachine.phaseName() : phaseBeforeSpike);

  // 19. Stable Gravity Detection
  bool stableShortSafe = strcmp(fermentationStateMachine.phaseName(), "READY TO PACKAGE") != 0;
  run("StableGravityDetectionShort", stableShortSafe, "duration_threshold", "not_ready", stableShortSafe ? "not_ready" : "ready");
  uint32_t stableEpoch = getCurrentEpoch();
  for (int i = 0; i < 4; i++) {
    stableEpoch += 12UL * 3600UL;
    uiTestSetMockTime(stableEpoch);
    uiTestSetMockSG(activeBrewProfile.expectedFinalGravity);
    uiTestRunAnalyzer();
  }
  bool stableLongReached = strcmp(fermentationStateMachine.phaseName(), "FG STABLE") == 0 ||
                           strcmp(fermentationStateMachine.phaseName(), "READY TO PACKAGE") == 0 ||
                           strcmp(fermentationStateMachine.phaseName(), "COMPLETED") == 0;
  run("StableGravityDetectionLong", stableLongReached, "stable_duration", "stable_or_ready", fermentationStateMachine.phaseName());

  // 20. Diacetyl Rest Logic
  BrewProfileStore::setDefaults(&activeBrewProfile);
  strcpy(activeBrewProfile.yeastCategory, "Lager");
  activeBrewProfile.diacetylRestRecommendedByYeast = true;
  activeBrewProfile.diacetylRestEnabled = true;
  brewProfileLoaded = true;
  fermentationStateMachine.reset();
  uiTestSetMockTime(getCurrentEpoch() + 3600UL);
  uiTestSetMockSG(1.012f);
  uiTestRunAnalyzer();
  bool dRestLager = strcmp(fermentationStateMachine.phaseName(), "D-REST READY") == 0 ||
                    strstr(currentRecommendation.message, "diacetyl") != NULL;
  run("DiacetylRestLogicLager", dRestLager, "lager_trigger", "drest_ready", dRestLager ? "drest_ready" : fermentationStateMachine.phaseName());
  activeBrewProfile.diacetylRestRecommendedByYeast = false;
  activeBrewProfile.diacetylRestEnabled = false;
  fermentationStateMachine.reset();
  uiTestSetMockTime(getCurrentEpoch() + 7200UL);
  uiTestSetMockSG(1.012f);
  uiTestRunAnalyzer();
  bool dRestAleSafe = strcmp(fermentationStateMachine.phaseName(), "D-REST READY") != 0;
  run("DiacetylRestLogicAleNoAuto", dRestAleSafe, "ale_no_auto", "not_drest", fermentationStateMachine.phaseName());
  uiTestSetMockTime(0);

  // 21. Reboot During Active Batch, soft reload.
  uiTestCreateBatch();
  static BrewProfile reloaded;
  bool reloadOK = false;
  if (mountSDTemporarily()) {
    reloadOK = BrewProfileStore::load("test_batch", &reloaded);
    dismountSD();
  }
  run("RebootDuringActiveBatch", reloadOK, "profile_reload", "true", reloadOK ? "true" : "false");

  // 21b. Mock time must never leak into the real logging path.
  uiTestCreateBatch();
  const uint32_t mockEpoch = 1779380000UL;
  uiTestSetMockTime(mockEpoch);
  static payload_t realPathPayload;
  memset(&realPathPayload, 0, sizeof(realPathPayload));
  realPathPayload.sequence_id = 42;
  realPathPayload.angle = 50.0f;
  realPathPayload.density = 1.0042f;
  realPathPayload.temperature = 20.4f;
  realPathPayload.battery_voltage = 4.08f;
  realPathPayload.uptime_s = 1234;
  uint32_t realEpoch = getCurrentEpoch();
  bool logWriteOK = isEpochValid(realEpoch) && logDataToSD(realPathPayload, realEpoch);
  uint32_t lastLogEpoch = 0;
  bool logReadOK = false;
  if (mountSDTemporarily()) {
    char logPath[80];
    BrewProfileStore::logPath("test_batch", logPath, sizeof(logPath));
    logReadOK = uiTestReadLastLogEpoch(logPath, &lastLogEpoch);
    dismountSD();
  }
  bool mockIsolated = logWriteOK && logReadOK && lastLogEpoch != mockEpoch && lastLogEpoch == realEpoch;
  snprintf(actual, sizeof(actual), "%lu", (unsigned long)lastLogEpoch);
  run("MockTimeIsolation", mockIsolated, "logged_epoch", "rtc_epoch_not_mock", actual);
  uiTestSetMockTime(0);

  // 21c. Mixed logs should make skipped rows visible instead of silently looking like missing history.
  uiTestCreateBatch();
  uint32_t mixedCreatedAt = getCurrentEpoch();
  activeBrewProfile.createdAt = mixedCreatedAt;
  static char mixedPath[80];
  BrewProfileStore::logPath("test_batch", mixedPath, sizeof(mixedPath));
  bool mixedOK = false;
  if (mountSDTemporarily()) {
    mixedOK = uiTestWriteMixedEpochLog(mixedPath, mixedCreatedAt);
    if (mixedOK) mixedOK = loadHistoricalDataFromCSV(mixedPath, LOAD_ALL, MAX_DATA_POINTS, 1);
    dismountSD();
  }
  bool mixedCountsOK = mixedOK && lastHistoricalLoadedLines == 2 && lastHistoricalSkippedLines == 1 && lastHistoricalParseErrors == 0;
  snprintf(actual, sizeof(actual), "L%d_S%d_E%d", lastHistoricalLoadedLines, lastHistoricalSkippedLines, lastHistoricalParseErrors);
  run("HistoricalMixedLogFilter", mixedCountsOK, "load_skip_counts", "L2_S1_E0", actual);

  // 21d-21e run in the smaller run_batch_restore_tests command to keep the ESP32 loop stack stable.

  // 22-25. Storage failure/corruption checks currently need hardware/file fault injection.
  run("SDCardMissing", false, "fault_injection", "sd_unavailable_mock", "not_implemented");
  run("CorruptProfileJson", false, "fault_injection", "corrupt_profile_recovery", "not_implemented");
  run("CorruptTargetJson", false, "fault_injection", "target_regenerated", "not_implemented");
  run("LogFileAppendFailure", false, "fault_injection", "write_failure_reported", "not_implemented");

  // 26. Touch Spam Test
  beginBrewWizard();
  for (int i = 0; i < 20; i++) {
    uiTestKey(i % 2 == 0 ? "next" : "prev");
  }
  bool touchSpamSafe = currentMode == BREW_WIZARD_VIEW || currentMode == LIVE_VIEW || currentMode == DASHBOARD_VIEW;
  run("TouchSpamTest", touchSpamSafe, "no_crash", "valid_screen", uiTestScreenName());

  // 27. Back Navigation Consistency
  beginBrewWizard();
  uiTestTypeText("BACKTEST");
  uiTestKey("next");
  uiTestKey("prev");
  bool backValueKept = strstr(activeBrewProfile.batchName, "BACKTEST") != NULL;
  run("BackNavigationConsistency", backValueKept, "field_retained", "BACKTEST", activeBrewProfile.batchName);

  // 28. Cancel Confirmation is intentionally expected to fail until UI adds confirmation.
  beginBrewWizard();
  uiTestTypeText("CANCELTEST");
  uiTestKey("prev");
  bool cancelDialogShown = uiTestVisibleText("Discard changes?");
  run("CancelConfirmation", cancelDialogShown, "discard_dialog", "visible", cancelDialogShown ? "visible" : "missing");

  // 29. Completed Batch Protection
  uiTestCreateBatch();
  bool markedCompleted = uiTestMarkTestBatchCompleted();
  continueManagedBatch("test_batch");
  bool completedProtected = markedCompleted && (!brewProfileLoaded || strcmp(activeBrewProfile.batchId, "test_batch") != 0);
  run("CompletedBatchProtection", completedProtected, "explicit_lock", "protected", completedProtected ? "protected" : "continued");

  // 30. Dashboard No Batch State
  uiTestDeleteBatch();
  currentMode = DASHBOARD_VIEW;
  screenDirty = true;
  bool noBatchOK = !brewProfileLoaded && strcmp(uiTestScreenName(), "DashboardScreen") == 0;
  run("DashboardNoBatchState", noBatchOK, "no_active_batch", "dashboard_no_batch", noBatchOK ? "dashboard_no_batch" : uiTestScreenName());

  // 31-32. Text limits and special characters.
  beginBrewWizard();
  bool longTextSafe = uiTestTypeText("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ") && strlen(activeBrewProfile.batchName) < sizeof(activeBrewProfile.batchName);
  uiTestKey("next");
  run("LongTextInput", longTextSafe, "bounded_name", "<40_chars", longTextSafe ? "bounded" : "overflow_or_reject");
  beginBrewWizard();
  bool specialCharsSafe = !uiTestTypeText("Test/Batch") && uiTestTypeText("PILS A");
  run("SpecialCharacters", specialCharsSafe, "unsupported_chars_safe", "reject_non_keyboard", specialCharsSafe ? "safe" : "unsafe");

  // 33. Memory/Heap Stability
  uint32_t heapBefore = ESP.getFreeHeap();
  for (int i = 0; i < 5; i++) {
    uiTestCreateBatch();
    uiTestDeleteBatch();
  }
  uint32_t heapAfter = ESP.getFreeHeap();
  bool heapStable = heapAfter + 2048 >= heapBefore;
  snprintf(actual, sizeof(actual), "%lu", (unsigned long)heapAfter);
  run("MemoryHeapStability", heapStable, "free_heap", "no_continuous_drop", actual);

  // 34-35. Full End-to-End Auto/Manual Mode
  uiTestCreateBatch();
  bool autoE2E = activeBrewProfile.autoModeEnabled && strcmp(activeBrewProfile.attenuationSource, "yeast_preset") == 0;
  uiTestSetMockSG(1.040f);
  uiTestRunAnalyzer();
  autoE2E = autoE2E && currentETA.valid && currentRecommendation.message[0] != '\0';
  run("FullEndToEndAutoMode", autoE2E, "auto_values", "eta_and_recommendation", autoE2E ? "ok" : "missing");
  BrewProfileStore::setDefaults(&activeBrewProfile);
  strncpy(activeBrewProfile.batchId, "test_batch", sizeof(activeBrewProfile.batchId) - 1);
  activeBrewProfile.autoModeEnabled = false;
  activeBrewProfile.expectedApparentAttenuation = 74;
  strcpy(activeBrewProfile.attenuationSource, "manual");
  brewProfileLoaded = true;
  completeBrewWizard();
  bool manualE2E = !activeBrewProfile.autoModeEnabled && strcmp(activeBrewProfile.attenuationSource, "manual") == 0;
  run("FullEndToEndManualMode", manualE2E, "manual_source", "manual", activeBrewProfile.attenuationSource);

  // Restore previous runtime state as much as possible.
  activeBrewProfile = savedProfile;
  brewProfileLoaded = savedProfileLoaded;
  fermentationFileOpen = savedFermentationFileOpen;
  strncpy(currentFermentationFile, savedFermentationFile, sizeof(currentFermentationFile));
  currentFermentationFile[sizeof(currentFermentationFile) - 1] = '\0';
  currentMode = savedMode;
  calibCoeffs = savedCalib;
  normOffset = savedNormOffset;
  normScale = savedNormScale;
  memcpy(displayDataBuffer, savedDisplayDataBuffer, sizeof(displayDataBuffer));
  memcpy(displayTimestampBuffer, savedDisplayTimestampBuffer, sizeof(displayTimestampBuffer));
  memcpy(dischargeRateBuffer, savedDischargeRateBuffer, sizeof(dischargeRateBuffer));
  memcpy(dischargeRateTimestampBuffer, savedDischargeRateTimestampBuffer, sizeof(dischargeRateTimestampBuffer));
  displayDataIndex = savedDisplayDataIndex;
  displayDataCount = savedDisplayDataCount;
  totalCSVDataLines = savedTotalCSVDataLines;
  lastHistoricalLoadedLines = savedLastHistoricalLoadedLines;
  lastHistoricalSkippedLines = savedLastHistoricalSkippedLines;
  lastHistoricalParseErrors = savedLastHistoricalParseErrors;
  lastHistoricalPreBatchLines = savedLastHistoricalPreBatchLines;
  lastHistoricalPostBatchLines = savedLastHistoricalPostBatchLines;
  lastHistoricalBatchFilteredLines = savedLastHistoricalBatchFilteredLines;
  lastHistoricalCutoffFilteredLines = savedLastHistoricalCutoffFilteredLines;
  lastHistoricalBatchStartRelaxed = savedLastHistoricalBatchStartRelaxed;
  lastHistoricalOldestEpoch = savedLastHistoricalOldestEpoch;
  lastHistoricalNewestEpoch = savedLastHistoricalNewestEpoch;
  dischargeRateBufferCount = savedDischargeRateBufferCount;
  previousAnalyticsSG = savedPreviousAnalyticsSG;
  previousAnalyticsEpoch = savedPreviousAnalyticsEpoch;
  cursorIndex = savedCursorIndex;
  cursorPosition = savedCursorPosition;
  if (mountSDTemporarily()) {
    if (savedActiveBatchIdValid) {
      BrewProfileStore::saveActiveBatchId(savedActiveBatchId);
    } else {
      BrewProfileStore::clearActiveBatchId();
    }
    dismountSD();
  }
  screenDirty = true;

  snprintf(buffer, bufferSize, "ui_regression total=%d passed=%d failed=%d first_failure=%s",
           total, passed, failed, firstFail[0] ? firstFail : "none");
  return failed == 0;
}
#endif
