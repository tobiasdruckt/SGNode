/*
 * Fermentation Monitor - Base Station
 * Receives data via ESP-NOW and displays on TFT with touch interface
 * Shows real-time data and historical graphs
 */

#include <WiFi.h>
#include <esp_now.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

// 4.0inch ESP32-32E Display configuration
#define SCREEN_W    320
#define SCREEN_H    240
#define DISPLAY_CS  5   // Chip Select (may vary based on module)
#define DISPLAY_DC  4   // Data/Command
#define DISPLAY_RST 2  // Reset

// Touch screen pins (integrated on ESP32-32E)
#define TOUCH_CS    15
#define TOUCH_IRQ   27

// SD card pins (4.0inch ESP32-32E has built-in SD card slot)
#define SD_CS       13  // SD card chip select (may vary based on module)

// ESP-NOW configuration
#define ESPNOW_CHANNEL 1

// Data structure for received data (must match float unit)
typedef struct {
  uint16_t sequence_id;    // Rolling sequence number
  uint32_t uptime_s;       // Uptime in seconds
  float angle;
  float density;
  float temperature;
  float battery_voltage;   // Battery voltage in volts
  uint8_t flags;           // Bitfield: bit0=delayed, bit1=sensor_err, bit2=low_batt
  uint16_t crc;            // CRC16 checksum
} payload_t;

// Calibration command structure
typedef struct {
  uint8_t command;        // 0=CALIBRATE_POINT1, 1=CALIBRATE_POINT2, 2=CALIBRATE_POINT3, 3=CALIBRATE_POINT4, 4=APPLY_CALIBRATION
  float target_sg;        // Target specific gravity for calibration point
  uint8_t request_id;     // Unique ID for response tracking
} calib_command_t;

// Calibration response structure
typedef struct {
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

// SD card and file management
char currentFermentationFile[32] = "";
bool fermentationFileOpen = false;
bool rebootPromptShown = false;
bool waitingForRebootChoice = false;

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
void drawLiveView();
void drawGraphView();
void drawCalibrationView();
void drawButtons();
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

void setup() {
  Serial.begin(115200);
  Serial.println("=== Fermentation Base Station Starting ===");
  
  initDisplay();
  initTouch();
  initSDCard();
  initESPNow();
  
  // Check for existing fermentation and show prompt if needed
  checkExistingFermentation();
  
  if (!waitingForRebootChoice) {
    Serial.println("Base station ready");
    // Initial screen draw
    drawButtons();
    drawLiveView();
  }
}

void loop() {
  // Check for touch input
  if (millis() - lastTouchCheck > TOUCH_CHECK_INTERVAL) {
    checkTouch();
    lastTouchCheck = millis();
  }
  
  // Update display based on current mode
  if (currentMode == LIVE_VIEW) {
    static unsigned long lastLiveUpdate = 0;
    if (millis() - lastLiveUpdate > 1000) { // Update every second
      drawLiveView();
      lastLiveUpdate = millis();
    }
  } else if (currentMode == GRAPH_VIEW) {
    static unsigned long lastGraphUpdate = 0;
    if (millis() - lastGraphUpdate > 5000) { // Update graph every 5 seconds
      drawGraphView();
      lastGraphUpdate = millis();
    }
  } else if (currentMode == CALIBRATION_VIEW) {
    // Calibration view updates on button presses only
  }
  
  delay(10);
}

void initESPNow() {
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Register receive callback
  esp_now_register_recv_cb(onDataReceived);
  
  Serial.println("ESP-NOW initialized");
}

void initDisplay() {
  tft.init();
  tft.setRotation(3); // Landscape mode
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  Serial.println("Display initialized");
}

void initTouch() {
  ts.begin();
  ts.setRotation(3);
  
  Serial.println("Touch screen initialized");
}

void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(payload_t)) {
    payload_t receivedData;
    memcpy(&receivedData, incomingData, len);
    
    // Verify CRC
    size_t payload_size = sizeof(receivedData) - sizeof(receivedData.crc);
    uint16_t calculated_crc = 0xFFFF;
    for (size_t i = 0; i < payload_size; i++) {
      calculated_crc ^= ((uint8_t*)&receivedData)[i];
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
    
    // Add data to buffer
    addDataPoint(receivedData);
    
    // Log to SD card
    logDataToSD(receivedData);
    
    // Update display immediately for live data
    if (currentMode == LIVE_VIEW) {
      drawLiveView();
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
    Serial.printf("Received invalid data size: %d bytes\n", len);
  }
}

void addDataPoint(payload_t data) {
  dataBuffer[dataIndex] = data;
  dataIndex = (dataIndex + 1) % MAX_DATA_POINTS;
  
  if (dataCount < MAX_DATA_POINTS) {
    dataCount++;
  }
}

void drawButtons() {
  // Live View button
  if (currentMode == LIVE_VIEW) {
    tft.fillRect(BUTTON_LIVE_X, BUTTON_LIVE_Y, BUTTON_LIVE_W, BUTTON_LIVE_H, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
  } else {
    tft.fillRect(BUTTON_LIVE_X, BUTTON_LIVE_Y, BUTTON_LIVE_W, BUTTON_LIVE_H, TFT_GRAY);
    tft.setTextColor(TFT_BLACK);
  }
  tft.drawRect(BUTTON_LIVE_X, BUTTON_LIVE_Y, BUTTON_LIVE_W, BUTTON_LIVE_H, TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(BUTTON_LIVE_X + 20, BUTTON_LIVE_Y + 10);
  tft.println("LIVE");
  
  // Graph View button
  if (currentMode == GRAPH_VIEW) {
    tft.fillRect(BUTTON_GRAPH_X, BUTTON_GRAPH_Y, BUTTON_GRAPH_W, BUTTON_GRAPH_H, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
  } else {
    tft.fillRect(BUTTON_GRAPH_X, BUTTON_GRAPH_Y, BUTTON_GRAPH_W, BUTTON_GRAPH_H, TFT_GRAY);
    tft.setTextColor(TFT_BLACK);
  }
  tft.drawRect(BUTTON_GRAPH_X, BUTTON_GRAPH_Y, BUTTON_GRAPH_W, BUTTON_GRAPH_H, TFT_WHITE);
  tft.setCursor(BUTTON_GRAPH_X + 20, BUTTON_GRAPH_Y + 10);
  tft.println("GRAPH");
  
  // Calibration View button
  if (currentMode == CALIBRATION_VIEW) {
    tft.fillRect(BUTTON_CALIB_X, BUTTON_CALIB_Y, BUTTON_CALIB_W, BUTTON_CALIB_H, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
  } else {
    tft.fillRect(BUTTON_CALIB_X, BUTTON_CALIB_Y, BUTTON_CALIB_W, BUTTON_CALIB_H, TFT_GRAY);
    tft.setTextColor(TFT_BLACK);
  }
  tft.drawRect(BUTTON_CALIB_X, BUTTON_CALIB_Y, BUTTON_CALIB_W, BUTTON_CALIB_H, TFT_WHITE);
  tft.setCursor(BUTTON_CALIB_X + 15, BUTTON_CALIB_Y + 10);
  tft.println("CALIB");
}

void drawLiveView() {
  // Clear live view area
  tft.fillRect(0, 50, SCREEN_W, SCREEN_H - 50, TFT_BLACK);
  
  // Draw calibration mode warning if active
  if (calibrationModeActive) {
    tft.setTextColor(TFT_RED);
    tft.setTextSize(1);
    tft.setCursor(10, 50);
    tft.println(" CALIBRATION MODE ACTIVE - HIGH BATTERY DRAIN ");
    tft.setCursor(10, 65);
    tft.println("Physical switch activated on float unit");
  }
  
  if (dataCount == 0) {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(50, calibrationModeActive ? 90 : 120);
    tft.println("Waiting for data...");
    return;
  }
  
  // Get latest data
  payload_t latest = dataBuffer[(dataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
  
  // Draw title
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.setCursor(10, calibrationModeActive ? 90 : 50);
  tft.println("FERMENTATION MONITOR - LIVE DATA");
  
  // Draw data values
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  
  // Density (SG)
  tft.setCursor(20, calibrationModeActive ? 110 : 80);
  tft.setTextColor(TFT_YELLOW);
  tft.print("SG: ");
  tft.setTextColor(TFT_WHITE);
  tft.printf("%.3f\n", latest.density);
  
  // Original Gravity (if captured)
  if (ogCaptured) {
    tft.setCursor(20, calibrationModeActive ? 140 : 110);
    tft.setTextColor(TFT_MAGENTA);
    tft.print("OG: ");
    tft.setTextColor(TFT_WHITE);
    tft.printf("%.3f\n", originalGravity);
  }
  
  // ABV (if OG captured)
  if (ogCaptured) {
    tft.setCursor(20, calibrationModeActive ? 170 : 140);
    tft.setTextColor(TFT_GREEN);
    tft.print("ABV: ");
    tft.setTextColor(TFT_WHITE);
    tft.printf("%.2f%%\n", currentABV);
  }
  
  // Tilt angle
  int angleY = calibrationModeActive ? 200 : 170;
  if (ogCaptured) angleY = calibrationModeActive ? 200 : 170;
  tft.setCursor(20, angleY);
  tft.setTextColor(TFT_CYAN);
  tft.print("Angle: ");
  tft.setTextColor(TFT_WHITE);
  tft.printf("%.1f%c\n", latest.angle, 248); // degree symbol
  
  // Temperature
  int tempY = angleY + 30;
  tft.setCursor(20, tempY);
  tft.setTextColor(TFT_ORANGE);
  tft.print("Temp: ");
  tft.setTextColor(TFT_WHITE);
  tft.printf("%.1f%cC\n", latest.temperature, 247); // degree symbol
  
  // Battery
  int battY = tempY + 30;
  uint8_t batteryPercent = calculateBatteryPercentage(latest.battery_voltage);
  tft.setCursor(20, battY);
  tft.setTextColor(TFT_ORANGE);
  tft.print("Battery: ");
  tft.setTextColor(TFT_WHITE);
  tft.printf("%d%%\n", batteryPercent);
  
  // Battery indicator bar
  int batteryBarX = 150;
  int batteryBarY = battY;
  int batteryBarW = 100;
  int batteryBarH = 20;
  
  tft.drawRect(batteryBarX, batteryBarY, batteryBarW, batteryBarH, TFT_WHITE);
  int fillWidth = (batteryPercent * batteryBarW) / 100;
  
  if (batteryPercent > 50) {
    tft.fillRect(batteryBarX + 2, batteryBarY + 2, fillWidth - 4, batteryBarH - 4, TFT_GREEN);
  } else if (batteryPercent > 20) {
    tft.fillRect(batteryBarX + 2, batteryBarY + 2, fillWidth - 4, batteryBarH - 4, TFT_YELLOW);
  } else {
    tft.fillRect(batteryBarX + 2, batteryBarY + 2, fillWidth - 4, batteryBarH - 4, TFT_RED);
  }
  
  // Last update time
  tft.setTextColor(TFT_GRAY);
  tft.setTextSize(1);
  int updateTimeY = battY + 30;
  tft.setCursor(20, updateTimeY);
  if (lastUpdate > 0) {
    unsigned long timeSinceUpdate = (millis() - lastUpdate) / 1000;
    tft.printf("Last update: %lu sec ago", timeSinceUpdate);
  } else {
    tft.println("No data received yet");
  }
  
  // Signal strength indicator
  tft.setCursor(20, updateTimeY + 15);
  if (millis() - lastUpdate < 5000) {
    tft.setTextColor(TFT_GREEN);
    tft.println("● Signal Strong");
  } else if (millis() - lastUpdate < 15000) {
    tft.setTextColor(TFT_YELLOW);
    tft.println("● Signal Weak");
  } else {
    tft.setTextColor(TFT_RED);
    tft.println("● No Signal");
  }
}

void drawGraphView() {
  // Clear graph area
  tft.fillRect(0, 50, SCREEN_W, SCREEN_H - 50, TFT_BLACK);
  
  if (dataCount < 2) {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(50, 120);
    tft.println("Need more data for graph");
    return;
  }
  
  // Draw title
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.setCursor(10, 50);
  tft.println("FERMENTATION MONITOR - HISTORICAL GRAPH");
  
  drawGrid();
  drawGraph();
}

void drawGrid() {
  // Graph area
  int graphX = 40;
  int graphY = 70;
  int graphW = SCREEN_W - 60;
  int graphH = SCREEN_H - 100;
  
  // Draw axes
  tft.drawFastHLine(graphX, graphY + graphH, graphW, TFT_WHITE); // X-axis
  tft.drawFastVLine(graphX, graphY, graphH, TFT_WHITE); // Y-axis
  
  // Draw grid lines
  tft.setTextColor(TFT_GRAY);
  tft.setTextSize(1);
  
  // Y-axis labels (SG values from 0.9 to 1.2)
  for (int i = 0; i <= 6; i++) {
    int y = graphY + graphH - (i * graphH / 6);
    tft.drawFastHLine(graphX, y, graphW, 0x03EF); // Dark cyan grid lines
    
    float sg = 0.9 + (i * 0.05);
    tft.setCursor(5, y - 3);
    tft.printf("%.2f", sg);
  }
  
  // X-axis labels (time markers)
  for (int i = 0; i <= 5; i++) {
    int x = graphX + (i * graphW / 5);
    tft.drawFastVLine(x, graphY, graphH, 0x03EF);
    
    tft.setCursor(x - 10, graphY + graphH + 2);
    tft.printf("%d", i * 20);
  }
  
  // Axis labels
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(graphX + graphW/2 - 20, graphY + graphH + 15);
  tft.println("Time (min)");
  
  // Rotate text for Y-axis label
  tft.setCursor(5, graphY + graphH/2 - 20);
  tft.println("SG");
}

void drawGraph() {
  if (dataCount < 2) return;
  
  int graphX = 40;
  int graphY = 70;
  int graphW = SCREEN_W - 60;
  int graphH = SCREEN_H - 100;
  
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
  
  // Draw density line
  int prevX = -1, prevY = -1;
  
  for (int i = 0; i < dataCount; i++) {
    payload_t data = dataBuffer[(dataIndex - dataCount + i + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    
    int x = graphX + (i * graphW / (MAX_DATA_POINTS - 1));
    int y = graphY + graphH - ((data.density - minDensity) / (maxDensity - minDensity) * graphH);
    
    if (prevX >= 0 && prevY >= 0) {
      tft.drawLine(prevX, prevY, x, y, TFT_YELLOW);
    }
    
    tft.drawPixel(x, y, TFT_RED);
    
    prevX = x;
    prevY = y;
  }
  
  // Draw current value indicator
  if (dataCount > 0) {
    payload_t latest = dataBuffer[(dataIndex - 1 + MAX_DATA_POINTS) % MAX_DATA_POINTS];
    
    int currentX = graphX + ((dataCount - 1) * graphW / (MAX_DATA_POINTS - 1));
    int currentY = graphY + graphH - ((latest.density - minDensity) / (maxDensity - minDensity) * graphH);
    
    tft.fillCircle(currentX, currentY, 3, TFT_GREEN);
    
    // Show current value
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(currentX + 10, currentY - 10);
    tft.printf("%.3f", latest.density);
  }
}

void drawCalibrationView() {
  // Clear calibration view area
  tft.fillRect(0, 50, SCREEN_W, SCREEN_H - 50, TFT_BLACK);
  
  // Draw title
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.setCursor(10, 50);
  tft.println("FERMENTATION MONITOR - CALIBRATION");
  
  // Draw calibration mode warning
  tft.setTextColor(TFT_RED);
  tft.setTextSize(1);
  tft.setCursor(10, 65);
  tft.println(" HIGH BATTERY DRAIN - PHYSICAL SWITCH ACTIVATED ");
  tft.setCursor(10, 80);
  tft.println("Float unit will not enter deep sleep mode");
  
  // Draw content based on calibration state
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  
  if (calibMode == CALIB_IDLE) {
    tft.setCursor(10, 100);
    tft.println("Press START to begin calibration");
    
    // Start button
    tft.fillRect(BUTTON_CALIB_START_X, BUTTON_CALIB_START_Y, BUTTON_CALIB_START_W, BUTTON_CALIB_START_H, TFT_GREEN);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(BUTTON_CALIB_START_X + 25, BUTTON_CALIB_START_Y + 12);
    tft.println("START");
    
  } else if (calibMode == CALIB_INSTRUCTIONS) {
    tft.setCursor(10, 100);
    tft.println("CALIBRATION INSTRUCTIONS:");
    tft.setCursor(10, 115);
    tft.println("1. Get a bowl (min 3L capacity)");
    tft.setCursor(10, 130);
    tft.println("2. Fill with 2L tap water");
    tft.setCursor(10, 145);
    tft.println("3. Place float in water");
    tft.setCursor(10, 160);
    tft.println("4. Wait for float to stabilize");
    
    // Next button
    tft.fillRect(BUTTON_CALIB_NEXT_X, BUTTON_CALIB_NEXT_Y, BUTTON_CALIB_NEXT_W, BUTTON_CALIB_NEXT_H, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(BUTTON_CALIB_NEXT_X + 30, BUTTON_CALIB_NEXT_Y + 12);
    tft.println("NEXT");
    
  } else if (calibMode == CALIB_POINT1) {
    tft.setCursor(10, 100);
    tft.println("STEP 1/4 - WATER (SG=1.000)");
    tft.setCursor(10, 115);
    tft.println("Float should be in 2L tap water");
    tft.setCursor(10, 130);
    tft.println("Wait for float to stabilize");
    tft.setCursor(10, 145);
    tft.println("Press RECORD when stable");
    
    // Record button
    tft.fillRect(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, 
                calibAngles[0] > 0 ? TFT_GREEN : TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(BUTTON_CALIB_RECORD_X + 25, BUTTON_CALIB_RECORD_Y + 12);
    tft.println("RECORD");
    
  } else if (calibMode == CALIB_POINT2) {
    tft.setCursor(10, 100);
    tft.println("STEP 2/4 - ADD SUGAR (SG=1.040)");
    tft.setCursor(10, 115);
    tft.printf("Add %dg normal sugar to bowl", sugarAmounts[1]);
    tft.setCursor(10, 130);
    tft.println("Swirl to dissolve completely");
    tft.setCursor(10, 145);
    tft.println("Wait for float to stabilize");
    tft.setCursor(10, 160);
    tft.println("Press RECORD when stable");
    
    // Record button
    tft.fillRect(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, 
                calibAngles[1] > 0 ? TFT_GREEN : TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(BUTTON_CALIB_RECORD_X + 25, BUTTON_CALIB_RECORD_Y + 12);
    tft.println("RECORD");
    
  } else if (calibMode == CALIB_POINT3) {
    tft.setCursor(10, 100);
    tft.println("STEP 3/4 - ADD SUGAR (SG=1.080)");
    tft.setCursor(10, 115);
    tft.printf("Add %dg normal sugar to bowl", sugarAmounts[2]);
    tft.setCursor(10, 130);
    tft.println("Swirl to dissolve completely");
    tft.setCursor(10, 145);
    tft.println("Wait for float to stabilize");
    tft.setCursor(10, 160);
    tft.println("Press RECORD when stable");
    
    // Record button
    tft.fillRect(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, 
                calibAngles[2] > 0 ? TFT_GREEN : TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(BUTTON_CALIB_RECORD_X + 25, BUTTON_CALIB_RECORD_Y + 12);
    tft.println("RECORD");
    
  } else if (calibMode == CALIB_POINT4) {
    tft.setCursor(10, 100);
    tft.println("STEP 4/4 - ADD SUGAR (SG=1.120)");
    tft.setCursor(10, 115);
    tft.printf("Add %dg normal sugar to bowl", sugarAmounts[3]);
    tft.setCursor(10, 130);
    tft.println("Swirl to dissolve completely");
    tft.setCursor(10, 145);
    tft.println("Wait for float to stabilize");
    tft.setCursor(10, 160);
    tft.println("Press RECORD when stable");
    
    // Record button
    tft.fillRect(BUTTON_CALIB_RECORD_X, BUTTON_CALIB_RECORD_Y, BUTTON_CALIB_RECORD_W, BUTTON_CALIB_RECORD_H, 
                calibAngles[3] > 0 ? TFT_GREEN : TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(BUTTON_CALIB_RECORD_X + 25, BUTTON_CALIB_RECORD_Y + 12);
    tft.println("RECORD");
    
  } else if (calibMode == CALIB_COMPLETE) {
    tft.setCursor(10, 100);
    tft.println("All 4 points recorded!");
    tft.setCursor(10, 115);
    tft.println("Press APPLY to calculate");
    tft.setCursor(10, 130);
    tft.println("3rd degree polynomial");
    
    // Show recorded angles
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(10, 150);
    tft.printf("P1: %.1f%c  P2: %.1f%c", calibAngles[0], 248, calibAngles[1], 248);
    tft.setCursor(10, 165);
    tft.printf("P3: %.1f%c  P4: %.1f%c", calibAngles[2], 248, calibAngles[3], 248);
    
    // Apply button
    tft.fillRect(BUTTON_CALIB_APPLY_X, BUTTON_CALIB_APPLY_Y, BUTTON_CALIB_APPLY_W, BUTTON_CALIB_APPLY_H, TFT_GREEN);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(BUTTON_CALIB_APPLY_X + 25, BUTTON_CALIB_APPLY_Y + 12);
    tft.println("APPLY");
  }
  
  // Exit button (always shown)
  tft.fillRect(BUTTON_CALIB_EXIT_X, BUTTON_CALIB_EXIT_Y, BUTTON_CALIB_EXIT_W, BUTTON_CALIB_EXIT_H, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(BUTTON_CALIB_EXIT_X + 25, BUTTON_CALIB_EXIT_Y + 8);
  tft.println("EXIT");
}

void checkTouch() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    touchX = map(p.x, 200, 3700, 0, SCREEN_W);
    touchY = map(p.y, 240, 3800, 0, SCREEN_H);
    
    // Handle reboot prompt buttons
    if (waitingForRebootChoice) {
      if (touchX >= BUTTON_CONTINUE_X && touchX <= BUTTON_CONTINUE_X + BUTTON_CONTINUE_W &&
          touchY >= BUTTON_CONTINUE_Y && touchY <= BUTTON_CONTINUE_Y + BUTTON_CONTINUE_H) {
        continueFermentationFile();
        waitingForRebootChoice = false;
        drawButtons();
        drawLiveView();
        Serial.println("Continuing existing fermentation");
      }
      
      if (touchX >= BUTTON_NEW_X && touchX <= BUTTON_NEW_X + BUTTON_NEW_W &&
          touchY >= BUTTON_NEW_Y && touchY <= BUTTON_NEW_Y + BUTTON_NEW_H) {
        createNewFermentationFile();
        waitingForRebootChoice = false;
        drawButtons();
        drawLiveView();
        Serial.println("Starting new fermentation");
      }
      
      delay(50); // Debounce
      return;
    }
    
    // Check button presses
    if (touchX >= BUTTON_LIVE_X && touchX <= BUTTON_LIVE_X + BUTTON_LIVE_W &&
        touchY >= BUTTON_LIVE_Y && touchY <= BUTTON_LIVE_Y + BUTTON_LIVE_H) {
      if (currentMode != LIVE_VIEW) {
        currentMode = LIVE_VIEW;
        drawButtons();
        drawLiveView();
        Serial.println("Switched to Live View");
      }
    }
    
    if (touchX >= BUTTON_GRAPH_X && touchX <= BUTTON_GRAPH_X + BUTTON_GRAPH_W &&
        touchY >= BUTTON_GRAPH_Y && touchY <= BUTTON_GRAPH_Y + BUTTON_GRAPH_H) {
      if (currentMode != GRAPH_VIEW) {
        currentMode = GRAPH_VIEW;
        drawButtons();
        drawGraphView();
        Serial.println("Switched to Graph View");
      }
    }
    
    if (touchX >= BUTTON_CALIB_X && touchX <= BUTTON_CALIB_X + BUTTON_CALIB_W &&
        touchY >= BUTTON_CALIB_Y && touchY <= BUTTON_CALIB_Y + BUTTON_CALIB_H) {
      if (currentMode != CALIBRATION_VIEW) {
        currentMode = CALIBRATION_VIEW;
        drawButtons();
        drawCalibrationView();
        Serial.println("Switched to Calibration View");
      }
    }
    
    // Handle calibration screen buttons
    if (currentMode == CALIBRATION_VIEW) {
      // Start button
      if (calibMode == CALIB_IDLE &&
          touchX >= BUTTON_CALIB_START_X && touchX <= BUTTON_CALIB_START_X + BUTTON_CALIB_START_W &&
          touchY >= BUTTON_CALIB_START_Y && touchY <= BUTTON_CALIB_START_Y + BUTTON_CALIB_START_H) {
        calibMode = CALIB_INSTRUCTIONS;
        drawCalibrationView();
        Serial.println("Started calibration wizard");
      }
      
      // Next button (instructions to step 1)
      if (calibMode == CALIB_INSTRUCTIONS &&
          touchX >= BUTTON_CALIB_NEXT_X && touchX <= BUTTON_CALIB_NEXT_X + BUTTON_CALIB_NEXT_W &&
          touchY >= BUTTON_CALIB_NEXT_Y && touchY <= BUTTON_CALIB_NEXT_Y + BUTTON_CALIB_NEXT_H) {
        calibMode = CALIB_POINT1;
        drawCalibrationView();
        Serial.println("Moved to Step 1");
      }
      
      // Record button for each step
      if (calibMode == CALIB_POINT1 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(0, 1.000);
        Serial.println("Sent calibration command for Point 1 (SG=1.000)");
      }
      
      if (calibMode == CALIB_POINT2 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(1, 1.040);
        Serial.println("Sent calibration command for Point 2 (SG=1.040)");
      }
      
      if (calibMode == CALIB_POINT3 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(2, 1.080);
        Serial.println("Sent calibration command for Point 3 (SG=1.080)");
      }
      
      if (calibMode == CALIB_POINT4 &&
          touchX >= BUTTON_CALIB_RECORD_X && touchX <= BUTTON_CALIB_RECORD_X + BUTTON_CALIB_RECORD_W &&
          touchY >= BUTTON_CALIB_RECORD_Y && touchY <= BUTTON_CALIB_RECORD_Y + BUTTON_CALIB_RECORD_H) {
        sendCalibrationCommand(3, 1.120);
        Serial.println("Sent calibration command for Point 4 (SG=1.120)");
      }
      
      // Apply button
      if (calibMode == CALIB_COMPLETE &&
          touchX >= BUTTON_CALIB_APPLY_X && touchX <= BUTTON_CALIB_APPLY_X + BUTTON_CALIB_APPLY_W &&
          touchY >= BUTTON_CALIB_APPLY_Y && touchY <= BUTTON_CALIB_APPLY_Y + BUTTON_CALIB_APPLY_H) {
        applyCalibration();
        drawCalibrationView();
        Serial.println("Applied calibration");
      }
      
      // Exit button
      if (touchX >= BUTTON_CALIB_EXIT_X && touchX <= BUTTON_CALIB_EXIT_X + BUTTON_CALIB_EXIT_W &&
          touchY >= BUTTON_CALIB_EXIT_Y && touchY <= BUTTON_CALIB_EXIT_Y + BUTTON_CALIB_EXIT_H) {
        currentMode = LIVE_VIEW;
        calibMode = CALIB_IDLE;
        // Reset calibration data
        for (int i = 0; i < 4; i++) calibAngles[i] = 0.0;
        drawButtons();
        drawLiveView();
        Serial.println("Exited calibration mode");
      }
    }
    
    delay(50); // Debounce
  }
}

void sendCalibrationCommand(uint8_t command, float target_sg) {
  // Send calibration command to float unit
  calib_command_t calibCmd;
  calibCmd.command = command;
  calibCmd.target_sg = target_sg;
  calibCmd.request_id = currentRequestId++;
  
  esp_now_send(NULL, (uint8_t*)&calibCmd, sizeof(calibCmd));
}

void applyCalibration() {
  // Apply calibration - send command to calculate polynomial coefficients
  calib_command_t calibCmd;
  calibCmd.command = 4; // APPLY_CALIBRATION
  calibCmd.target_sg = 0.0;
  calibCmd.request_id = currentRequestId++;
  
  esp_now_send(NULL, (uint8_t*)&calibCmd, sizeof(calibCmd));
}

void onCalibrationResponse(const uint8_t *mac, const uint8_t *incomingData, int len) {
  // Handle calibration response from float unit
  calib_response_t calibResp;
  memcpy(&calibResp, incomingData, len);
  
  Serial.printf("Calibration response: angle=%.2f°, sg=%.3f, request_id=%d\n", 
                calibResp.angle, calibResp.sg, calibResp.request_id);
  
  if (calibResp.response_type == 0) {
    // Calibration data - store angle based on which point was calibrated
    if (calibResp.request_id == currentRequestId - 1) {
      // Determine which calibration point this response is for based on target_sg
      if (calibResp.sg >= 0.995 && calibResp.sg <= 1.005) {
        // Point 1 - Water (SG=1.000)
        calibAngles[0] = calibResp.angle;
        Serial.printf("Stored Point 1 angle: %.2f°\n", calibAngles[0]);
        calibMode = CALIB_POINT2;
        drawCalibrationView();
      } else if (calibResp.sg >= 1.035 && calibResp.sg <= 1.045) {
        // Point 2 - Light Sugar (SG=1.040)
        calibAngles[1] = calibResp.angle;
        Serial.printf("Stored Point 2 angle: %.2f°\n", calibAngles[1]);
        calibMode = CALIB_POINT3;
        drawCalibrationView();
      } else if (calibResp.sg >= 1.075 && calibResp.sg <= 1.085) {
        // Point 3 - Medium Sugar (SG=1.080)
        calibAngles[2] = calibResp.angle;
        Serial.printf("Stored Point 3 angle: %.2f°\n", calibAngles[2]);
        calibMode = CALIB_POINT4;
        drawCalibrationView();
      } else if (calibResp.sg >= 1.115 && calibResp.sg <= 1.125) {
        // Point 4 - Heavy Sugar (SG=1.120)
        calibAngles[3] = calibResp.angle;
        Serial.printf("Stored Point 4 angle: %.2f°\n", calibAngles[3]);
        calibMode = CALIB_COMPLETE;
        drawCalibrationView();
      }
    }
  }
}

void initSDCard() {
  Serial.println("Initializing SD card...");
  
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    tft.setTextColor(TFT_RED);
    tft.setTextSize(1);
    tft.setCursor(10, 100);
    tft.println("SD Card Failed!");
    return;
  }
  
  Serial.println("SD card initialized successfully");
  
  // Check if fermentation directory exists
  if (!SD.exists("/fermentation")) {
    SD.mkdir("/fermentation");
    Serial.println("Created fermentation directory");
  }
}

void checkExistingFermentation() {
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card not available, skipping fermentation check");
    return;
  }
  
  // Check for existing fermentation files
  File root = SD.open("/fermentation");
  if (!root) {
    Serial.println("Failed to open fermentation directory");
    return;
  }
  
  File file = root.openNextFile();
  bool hasExistingFiles = false;
  
  while (file) {
    if (!file.isDirectory()) {
      hasExistingFiles = true;
      // Get the most recent file
      strcpy(currentFermentationFile, file.name());
      Serial.printf("Found existing fermentation file: %s\n", currentFermentationFile);
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
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  
  // Title
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(40, 30);
  tft.println("Fermentation");
  tft.setCursor(60, 55);
  tft.println("Found!");
  
  // Question
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, 90);
  tft.println("Continue existing");
  tft.setCursor(50, 105);
  tft.println("or start new?");
  
  // Continue button
  tft.fillRect(BUTTON_CONTINUE_X, BUTTON_CONTINUE_Y, BUTTON_CONTINUE_W, BUTTON_CONTINUE_H, TFT_GREEN);
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(BUTTON_CONTINUE_X + 15, BUTTON_CONTINUE_Y + 12);
  tft.println("CONTINUE");
  
  // New button
  tft.fillRect(BUTTON_NEW_X, BUTTON_NEW_Y, BUTTON_NEW_W, BUTTON_NEW_H, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(BUTTON_NEW_X + 25, BUTTON_NEW_Y + 12);
  tft.println("NEW");
  
  // File name
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(40, 190);
  tft.print("File: ");
  tft.print(currentFermentationFile);
}

void createNewFermentationFile() {
  // Generate filename with timestamp
  char filename[32];
  sprintf(filename, "/fermentation/ferm_%ld.csv", millis());
  
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to create new fermentation file");
    return;
  }
  
  // Write CSV header
  file.println("uptime_s,angle,density,temperature,battery_voltage,battery_percent");
  file.close();
  
  strcpy(currentFermentationFile, filename);
  fermentationFileOpen = true;
  
  Serial.printf("Created new fermentation file: %s\n", currentFermentationFile);
}

void continueFermentationFile() {
  fermentationFileOpen = true;
  Serial.printf("Continuing fermentation file: %s\n", currentFermentationFile);
}

void logDataToSD(payload_t data) {
  if (!fermentationFileOpen || strlen(currentFermentationFile) == 0) {
    return;
  }
  
  File file = SD.open(currentFermentationFile, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open fermentation file for writing");
    return;
  }
  
  // Write CSV data
  uint8_t batteryPercent = calculateBatteryPercentage(data.battery_voltage);
  file.printf("%lu,%.2f,%.4f,%.2f,%.2f,%d\n", 
             data.uptime_s, data.angle, data.density, data.temperature, 
             data.battery_voltage, batteryPercent);
  
  Serial.printf("Logged data to SD: SG=%.4f, Temp=%.2f°C, Battery=%.2fV (%d%%)\n", 
               data.density, data.temperature, data.battery_voltage, batteryPercent);
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

void logOGToSD() {
  if (!fermentationFileOpen || strlen(currentFermentationFile) == 0) {
    return;
  }
  
  File file = SD.open(currentFermentationFile, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open fermentation file for OG logging");
    return;
  }
  
  // Write OG as a comment line in the CSV file
  file.printf("# OG_CAPTURED: %.4f at reading #%d\n", originalGravity, readingCount);
  
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