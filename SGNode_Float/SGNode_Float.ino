/*
 * Fermentation Monitor - Float Unit (Sensor)
 * Measures tilt via BMI160 IMU and transmits via ESP-NOW
 * Battery-powered with deep sleep for power efficiency
 * 
 * REQUIRED LIBRARIES (install via Arduino Library Manager):
 * - EmotiBit BMI160 (by Connected Future Labs) - Install from:
 *   https://github.com/EmotiBit/EmotiBit_BMI160
 *   Download as ZIP and install via Sketch -> Include Library -> Add .ZIP Library
 * - Wire (built-in)
 * - EEPROM (built-in)
 */

#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>
#include <Update.h>
#include <Wire.h>
#include <BMI160Gen.h>
#include "polynomial_calibration.h"
#include "../SGNode_Shared/sg_protocol.h"
#include "../SGNode_Shared/sg_logo_png.h"

// I2C pins for BMI160
#define I2C_SDA 26
#define I2C_SCL 27

// System configuration
#define MEASUREMENT_INTERVAL 600  // seconds (configurable 60-600)
#define SAMPLE_COUNT 5            // samples per measurement (reduced for power efficiency)
#define MAX_RETRIES 1             // transmission retry attempts
#define MAX_INIT_RETRIES 3        // sensor initialization retry attempts
#define BATTERY_PIN 15            // Battery voltage monitoring (GPIO15 - ADC1_CH3)
#define CALIBRATION_SWITCH_PIN 12  // Physical switch for calibration mode (pull-down)
#define ACK_PACKET_TYPE 0xA5
#define RETRY_BUFFER_SIZE 8
#define ACK_WAIT_MS 400
#ifndef SGNODE_FLOAT_TEST_HARNESS
#define SGNODE_FLOAT_TEST_HARNESS 0
#endif
#ifndef SGNODE_FLOAT_SERIAL_DIAG
#define SGNODE_FLOAT_SERIAL_DIAG 0
#endif

// LED indicators (WeMos D32)
#define LED_BUILTIN 16           // Built-in LED - blinks during calibration mode

// ESP-NOW configuration
#define ESPNOW_CHANNEL 1

// Base station MAC address (update with actual address)
uint8_t baseStationMac[] = {0xA4, 0xF0, 0x0F, 0x68, 0x22, 0x00};

payload_t sensorData;

struct StoredPayload {
  bool valid;
  payload_t payload;
};

RTC_DATA_ATTR StoredPayload retryBuffer[RETRY_BUFFER_SIZE];
RTC_DATA_ATTR uint8_t retryWriteIndex = 0;
RTC_DATA_ATTR uint16_t lastAckedSeq = 0;
RTC_DATA_ATTR uint16_t rtcSequenceCounter = 0;
volatile bool ackReceived = false;
volatile uint16_t receivedAckSeq = 0;
RTC_DATA_ATTR uint8_t rtcPendingPayloadFlags = 0;
RTC_DATA_ATTR uint8_t rtcLastAckCommandId = 0;
volatile bool queuedZeroCalCommand = false;
volatile uint8_t queuedZeroCalCommandId = 0;
bool zeroCalProcessedThisWake = false;
#if SGNODE_FLOAT_TEST_HARNESS
bool harnessNoSleep = true;
bool harnessPauseStateMachine = true;
bool harnessMockEnabled = false;
bool harnessRxDebug = false;
uint32_t harnessLastIdlePrint = 0;
float harnessMockAngle = 50.0f;
float harnessMockTemp = 20.0f;
float harnessMockBattery = 4.0f;
float harnessMockSG = 1.000f;
char harnessCommandBuffer[128];
size_t harnessCommandLength = 0;
#endif

// BMI160 instance provided by EmotiBit_BMI160 library
// No need to declare - library provides global BMI160 instance

// Calibration system variables (actual definitions)
CalibrationCoefficients calibCoeffs;
CalibrationPoint calibPoints[MAX_CALIB_POINTS];
int numCalibPoints = 0;

// Sensor offset calibration variables
SensorOffsets sensorOffsets;

// Global variables for filtering and tracking
uint16_t sequence_counter = 0;
uint32_t boot_time = 0;
float ema_temp = 20.0;  // Initial temperature for EMA
const float EMA_ALPHA = 0.2;  // EMA alpha between 0.1-0.3
bool debug_mode = false;  // Debug mode enabled by calibration switch
float cachedBatteryVoltage = 0.0;  // Battery voltage read before WiFi

// State machine states
enum State {
  INIT,
  MEASURE,
  COMPUTE,
  SEND,
  SLEEP
};

// Calibration states
enum CalibState {
  CALIB_IDLE,
  CALIB_MEASURING,
  CALIB_WAITING_COMMAND
};

State currentState = INIT;
CalibState calibState = CALIB_IDLE;

// Calibration mode
bool calibrationMode = false;
uint8_t pendingRequestId = 0;
bool calibrationInProgress = false;

// Push button state tracking
bool lastButtonState = HIGH;
unsigned long lastButtonDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50; // 50ms debounce

// Calibration timeout system
unsigned long lastCalibrationActivity = 0;
const unsigned long CALIBRATION_TIMEOUT = 900000; // 15 minutes in milliseconds
const unsigned long CALIBRATION_NO_COMM_TIMEOUT = 60000; // 1 minute timeout for no communication

// LED blinking for calibration mode
unsigned long lastLedToggle = 0;
const unsigned long LED_BLINK_INTERVAL = 500; // 500ms blink interval
bool ledState = false;
const unsigned long OTA_LONG_PRESS_MS = 5000UL;
const unsigned long OTA_TIMEOUT_MS = 20UL * 60UL * 1000UL;
WebServer floatOtaServer(80);
bool floatOtaActive = false;
bool floatOtaUploadInProgress = false;
unsigned long floatOtaStartedMs = 0;
unsigned long floatOtaButtonPressMs = 0;
bool floatOtaButtonWasPressed = false;
char floatOtaSsid[32] = "";

// Rate limiting for calibration responses
unsigned long lastCalibrationResponse = 0;
const unsigned long CALIBRATION_RESPONSE_INTERVAL = 2000; // 2 seconds between responses
bool calibrationResponsePending = false;

struct PendingStableCalibrationPoint {
  bool active;
  float targetSG;
  uint8_t requestId;
  uint8_t pointNumber;
  unsigned long startedMs;
  unsigned long lastSampleMs;
  int sampleCount;
  float firstAngle;
  float firstTemp;
  float firstSG;
  float lastAngle;
  float lastTemp;
  float lastSG;
  float sumAngle;
  float sumTemp;
};

PendingStableCalibrationPoint pendingStablePoint = {};

struct PendingStableOffsetCalibration {
  bool active;
  uint8_t requestId;
  unsigned long startedMs;
  unsigned long lastSampleMs;
  int sampleCount;
  float firstX;
  float firstY;
  float firstZ;
  float lastX;
  float lastY;
  float lastZ;
  float firstTemp;
  float lastTemp;
  float previousTempDrift;
  unsigned long previousTempDriftMs;
  float peakTempDrift;
  float sumX;
  float sumY;
  float sumZ;
  float sumTemp;
  int minX;
  int minY;
  int minZ;
  int maxX;
  int maxY;
  int maxZ;
};

PendingStableOffsetCalibration pendingStableOffset = {};

// Function prototypes
void initIMU();
void initTemperatureSensor();
void initESPNow();
void configureUnusedGPIOs();
void suspendBMI160();
void ensureDebugSerial();
bool detectOtaLongPressAtBoot(bool switchPressed);
void startFloatOtaMode();
void handleFloatOtaMode();
void stopFloatOtaAndReboot();
void sendFloatOtaActiveStatus();
float measureTilt();
float measureImuTemperature();
float measureTemperature();
float calculateDensity(float angle, float temperature);
float getBatteryVoltage();
void computeSensorData();
void transmitData();
void onDataAck(const uint8_t *mac, const uint8_t *incomingData, int len);
void storePayloadForRetry(const payload_t& payload);
void markAcked(uint16_t sequence_id);
void sendRetryPayloads(bool includeCurrent = false);
bool sendPayloadWithAck(payload_t* payload, bool waitForAck);
void enterDeepSleep();
void onCalibrationCommand(const uint8_t *mac, const uint8_t *incomingData, int len);
void recordCalibrationPoint(float target_sg, uint8_t request_id, uint8_t point_number);
void startStableOffsetCalibration(uint8_t request_id);
void handlePendingStableOffsetCalibration();
void handlePendingStableCalibrationPoint();
bool runQuickZeroCalibration();
bool processQueuedZeroCalibrationCommand();
bool attachPendingFlagsToPayload(payload_t* payload, bool refreshMeasurement);
bool sendPendingZeroCalibrationResult();
void sendCalibrationResponse(float angle, float target_sg, uint8_t request_id, const char* message);
void sendCalibrationStatus(float etaSeconds, float tempDriftPerMin, uint8_t request_id, const char* message);
void sendCalibrationCoefficients(uint8_t request_id);
uint16_t crc16(const uint8_t* data, size_t length);
float ema(float prev, float x, float alpha);
#if SGNODE_FLOAT_TEST_HARNESS
void handleFloatTestHarness();
bool processFloatHarnessCommand(char* line);
int retryBufferCount();
void printFloatHarnessOK(const char* message);
void printFloatHarnessERR(const char* message);
#endif

void setup() {
  // Configure unused GPIOs FIRST to avoid conflicts
  configureUnusedGPIOs();
  
  // Configure pins (after unused GPIOs are set)
  pinMode(BATTERY_PIN, INPUT);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);  // Pin 15 is ADC1_CH3 - can use attenuation
  
  // Configure ADC for higher accuracy battery measurements
  analogReadResolution(12);  // Use 12-bit resolution (0-4095)
  // ESP32 Arduino doesn't have analogSetCycles/analogSetSamples functions
  // We'll use our own averaging in the measurement function
  pinMode(CALIBRATION_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Initialize LED
  digitalWrite(LED_BUILTIN, LOW);
  
  // Check wake-up reason to detect calibration switch press during deepsleep
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool calibrationWakeup = false;
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // Woke up from deepsleep due to external interrupt (calibration switch)
    calibrationWakeup = true;
    if (debug_mode) Serial.println("Woke up from deepsleep - calibration switch detected");
  }
  
  // Read battery voltage BEFORE WiFi is enabled (no interference)
  cachedBatteryVoltage = getBatteryVoltage();
  if (debug_mode) Serial.printf("Initial battery voltage: %.2fV\n", cachedBatteryVoltage);
  
  // Initialize push button state
  lastButtonState = digitalRead(CALIBRATION_SWITCH_PIN);
  
  // Check calibration switch state at boot (before deepsleep)
  bool switchPressed = (digitalRead(CALIBRATION_SWITCH_PIN) == LOW);

  if (detectOtaLongPressAtBoot(switchPressed)) {
    startFloatOtaMode();
    return;
  }
  
  // Activate calibration mode if switch is pressed OR woke up from calibration interrupt
  if (switchPressed || calibrationWakeup) {
    calibrationMode = true;
  } else {
    calibrationMode = false;
  }
  
  debug_mode = calibrationMode;  // Debug mode follows calibration mode
  
  // Start Serial in debug/calibration mode or when diagnostics are compiled in.
  if (debug_mode || SGNODE_FLOAT_TEST_HARNESS || SGNODE_FLOAT_SERIAL_DIAG) {
    Serial.begin(115200);
    delay(100);  // Wait for Serial to initialize

    if (SGNODE_FLOAT_SERIAL_DIAG) {
      Serial.printf("DIAG boot calib=%d switch=%d wake=%d state=%d\n",
                    calibrationMode ? 1 : 0, switchPressed ? 1 : 0,
                    calibrationWakeup ? 1 : 0, (int)currentState);
    }

    if (debug_mode) {
      Serial.println("=== CALIBRATION MODE ACTIVATED AT BOOT ===");
      if (switchPressed) {
        Serial.println("Switch pressed at boot");
      } else {
        Serial.println("Woke up from calibration interrupt");
      }

      Serial.printf("Base Station MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    baseStationMac[0], baseStationMac[1], baseStationMac[2],
                    baseStationMac[3], baseStationMac[4], baseStationMac[5]);
    }
  }
  
  if (calibrationMode) {
    lastCalibrationActivity = millis();
    
    // IMPORTANT: Send calibration trigger after ESP-NOW is initialized
    // This will be called in the state machine after initESPNow()
  }
  
  // Disable Bluetooth
  btStop();
  delay(50);
  
  // Initialize calibration system
  initCalibration();
  
  // Initialize sensor offset system
  initSensorOffsets();

  #if SGNODE_FLOAT_TEST_HARNESS
  Serial.println("OK float_harness_boot continuing_to_imu_init");
  #endif
  
  // Configure I2C with pull-ups
  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, INPUT_PULLUP);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000); // 100 kHz I2C
  
  // Check I2C bus is working
  Wire.beginTransmission(0x69); // BMI160 actual address (found by scanner)
  if (Wire.endTransmission() != 0) {
    if (debug_mode) Serial.println("I2C bus check failed - no device detected");
  } else {
    if (debug_mode) Serial.println("I2C bus initialized successfully");
  }
  
  // Record boot time
  boot_time = millis() / 1000;
  
  if (calibrationMode) {
    if (debug_mode) Serial.println("Button activated - Deep sleep disabled");
        lastCalibrationActivity = millis(); // Initialize timeout counter
    if (isCalibrationValid()) {
      if (debug_mode) Serial.println("Valid calibration found from EEPROM");
      if (debug_mode) printCalibrationCoefficients();
    } else {
      if (debug_mode) Serial.println("No valid calibration - ready for new calibration");
    }
  }
  
  if (debug_mode) Serial.println("Hardware initialized");

  #if SGNODE_FLOAT_TEST_HARNESS
  initIMU();
  Serial.println("OK float_harness_ready paused=1 imu=1");
  #endif
}

void loop() {
  if (floatOtaActive) {
    handleFloatOtaMode();
    delay(5);
    return;
  }

  #if SGNODE_FLOAT_TEST_HARNESS
  handleFloatTestHarness();
  if (harnessPauseStateMachine) {
    delay(10);
    return;
  }
  #endif
  
  // Ensure debug_mode follows calibration mode
  debug_mode = calibrationMode;

  #if SGNODE_FLOAT_SERIAL_DIAG
  static unsigned long lastDiagPrint = 0;
  if (millis() - lastDiagPrint > 5000UL) {
    lastDiagPrint = millis();
    Serial.printf("DIAG loop calib=%d state=%d button=%d pendingOffset=%d pendingPoint=%d\n",
                  calibrationMode ? 1 : 0, (int)currentState,
                  digitalRead(CALIBRATION_SWITCH_PIN),
                  pendingStableOffset.active ? 1 : 0,
                  pendingStablePoint.active ? 1 : 0);
  }
  #endif
  
  // Handle push button detection with debouncing
  bool currentButtonState = digitalRead(CALIBRATION_SWITCH_PIN);
  
  // Debug button state (remove after testing) - only in non-calibration mode
  static unsigned long lastDebugPrint = 0;
  if (!calibrationMode && debug_mode && millis() - lastDebugPrint > 1000) {
    Serial.printf("Button state: %d (pin %d)\n", currentButtonState, CALIBRATION_SWITCH_PIN);
    lastDebugPrint = millis();
  }
  
  if (currentButtonState != lastButtonState) {
    lastButtonDebounceTime = millis();
  }
  
  // Check if button state is stable (debounced)
  if ((millis() - lastButtonDebounceTime) > DEBOUNCE_DELAY) {
    static bool stableButtonState = HIGH;
    if (currentButtonState != stableButtonState) {
      stableButtonState = currentButtonState;
      if (stableButtonState == LOW) {
        // Button pressed (falling edge detected)
        if (debug_mode) Serial.println("Button press detected!");
        toggleCalibrationMode();
      }
    }
  }
  
  lastButtonState = currentButtonState;
  
  // Handle calibration timeout
  if (calibrationMode && (millis() - lastCalibrationActivity) > CALIBRATION_TIMEOUT) {
    if (debug_mode) Serial.println("Calibration timeout - exiting calibration mode");
    exitCalibrationMode();
  }
  
  // Handle no-communication timeout (exit if no base station communication for 1 minute)
  if (calibrationMode && (millis() - lastCalibrationActivity) > CALIBRATION_NO_COMM_TIMEOUT) {
    static bool noCommWarningShown = false;
    if (!noCommWarningShown) {
      if (debug_mode) Serial.println("Warning: No base station communication detected");
      if (debug_mode) Serial.println("Press button again to exit calibration mode");
      noCommWarningShown = true;
    }
  }
  
  // Handle LED blinking during calibration mode
  if (calibrationMode) {
    if ((millis() - lastLedToggle) > LED_BLINK_INTERVAL) {
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
      lastLedToggle = millis();
    }
  } else {
    digitalWrite(LED_BUILTIN, LOW); // Ensure LED is off when not in calibration mode
  }
  
  // Normal operation
  switch (currentState) {
    case INIT:
      if (debug_mode) Serial.println("State: INIT");
      delay(100);   // Short stabilization delay after wake
      initIMU();
      initTemperatureSensor();
      // WiFi initialization moved to SEND state for power savings
      
      // Send calibration trigger if in calibration mode and ESP-NOW is ready
      if (calibrationMode) {
        initESPNow();  // Only init WiFi in calibration mode
        static bool calibrationTriggerSent = false;
        if (!calibrationTriggerSent) {
          sendCalibrationTrigger();
          calibrationTriggerSent = true;
          if (debug_mode) Serial.println("Calibration trigger sent after ESP-NOW initialization");
        }
      }
      
      currentState = MEASURE;
      break;
      
    case MEASURE:
      sensorData.angle = measureTilt();
      currentState = COMPUTE;
      break;
      
    case COMPUTE:
      computeSensorData();
      currentState = SEND;
      break;
      
    case SEND:
      // Initialize WiFi only when needed for transmission (power savings)
      if (!calibrationMode) {
        initESPNow();  // Initialize WiFi/ESP-NOW only for transmission
        if (debug_mode) Serial.println("Transmitting normal sensor data");
        transmitData();
        currentState = SLEEP;
      } else {
        // In calibration mode, check for pending responses and continue measuring
        handlePendingStableOffsetCalibration();

        static unsigned long lastCalibMsg = 0;
        if (millis() - lastCalibMsg > 10000) { // Only show message every 10 seconds
          if (debug_mode) Serial.println("Calibration mode active - waiting for commands...");
          lastCalibMsg = millis();
        }
        if (calibrationResponsePending) {
          // Try to send pending calibration response
          if (millis() - lastCalibrationResponse >= CALIBRATION_RESPONSE_INTERVAL) {
            // Reset rate limiting to allow pending response
            lastCalibrationResponse = 0;
            calibrationResponsePending = false;
            if (debug_mode) Serial.println("Retrying pending calibration response");
          }
        }
        currentState = MEASURE;
        delay(1000); // Slower cycle in calibration mode
      }
      break;
      
    case SLEEP:
      if (debug_mode) Serial.println("State: SLEEP");
      enterDeepSleep();
      break;
  }
  
  delay(10);
}

bool detectOtaLongPressAtBoot(bool switchPressed) {
  if (!switchPressed) return false;
  unsigned long startMs = millis();
  while (digitalRead(CALIBRATION_SWITCH_PIN) == LOW) {
    if (millis() - lastLedToggle > 200UL) {
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
      lastLedToggle = millis();
    }
    if (millis() - startMs >= OTA_LONG_PRESS_MS) {
      digitalWrite(LED_BUILTIN, LOW);
      return true;
    }
    delay(10);
  }
  digitalWrite(LED_BUILTIN, LOW);
  return false;
}

String floatOtaDiagHtml() {
  String html;
  html.reserve(1200);
  uint32_t elapsed = (millis() - floatOtaStartedMs) / 1000UL;
  uint32_t remain = elapsed < (OTA_TIMEOUT_MS / 1000UL) ? (OTA_TIMEOUT_MS / 1000UL) - elapsed : 0UL;
  html += "<pre>";
  html += "Firmware: SGNode Float ";
  html += __DATE__;
  html += " ";
  html += __TIME__;
  html += "\nChip: ";
  html += String((uint32_t)ESP.getEfuseMac(), HEX);
  html += "\nMAC: ";
  html += WiFi.macAddress();
  html += "\nFlash: ";
  html += String(ESP.getFlashChipSize());
  html += "\nSketch: ";
  html += String(ESP.getSketchSize());
  html += "\nFree sketch: ";
  html += String(ESP.getFreeSketchSpace());
  html += "\nFree heap: ";
  html += String(ESP.getFreeHeap());
  html += "\nUptime: ";
  html += String(millis() / 1000UL);
  html += " s\nReset: ";
  html += String((int)esp_reset_reason());
  html += "\nAP SSID: ";
  html += floatOtaSsid;
  html += "\nAP IP: ";
  html += WiFi.softAPIP().toString();
  html += "\nOTA rest: ";
  html += String(remain);
  html += " s\nBattery: ";
  html += String(cachedBatteryVoltage, 2);
  html += " V\nSeq: ";
  html += String(rtcSequenceCounter);
  html += "\nCalibration: ";
  html += calibrationMode ? "on" : "off";
  html += "\nRetry buffer: ";
  int retryCount = 0;
  for (int i = 0; i < RETRY_BUFFER_SIZE; i++) {
    if (retryBuffer[i].valid) retryCount++;
  }
  html += String(retryCount);
  html += "</pre>";
  return html;
}

String floatOtaPageHtml() {
  String html;
  html.reserve(2200);
  html += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>SGNode Float OTA</title><style>body{font-family:sans-serif;margin:24px;max-width:720px}";
  html += "button,input{font-size:16px;padding:10px;margin:6px 0}pre{background:#f3f4f6;padding:12px;overflow:auto}";
  html += ".hdr{display:flex;align-items:center;gap:14px;margin-bottom:16px}.logo{width:96px;height:auto}</style></head><body>";
  html += "<div class='hdr'><img class='logo' src='/logo.png' alt='SGNode'><h2>Float OTA</h2></div>";
  html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
  html += "<input type='file' name='update'><br><button type='submit'>Upload firmware</button></form>";
  html += "<p><a href='/diag'>Refresh diagnostics</a> | <a href='/reboot'>Reboot normal mode</a></p>";
  html += floatOtaDiagHtml();
  html += "</body></html>";
  return html;
}

void sendFloatOtaActiveStatus() {
  initESPNow();
  sg_float_ota_status_t status = {};
  status.packet_type = SG_FLOAT_OTA_STATUS_TYPE;
  status.version = SG_PROTOCOL_VERSION;
  status.sequence_id = rtcSequenceCounter++;
  status.uptime_s = millis() / 1000UL;
  strncpy(status.ssid, floatOtaSsid, sizeof(status.ssid) - 1);
  strncpy(status.ip, "192.168.4.1", sizeof(status.ip) - 1);
  status.timeout_s = OTA_TIMEOUT_MS / 1000UL;
  status.crc = sg_crc16((const uint8_t*)&status, sizeof(status) - sizeof(status.crc));
  esp_now_send(baseStationMac, (uint8_t*)&status, sizeof(status));
  delay(80);
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
}

void startFloatOtaMode() {
  ensureDebugSerial();
  debug_mode = true;
  calibrationMode = false;
  floatOtaActive = true;
  floatOtaUploadInProgress = false;
  floatOtaStartedMs = millis();
  snprintf(floatOtaSsid, sizeof(floatOtaSsid), "SGNode-Float-OTA-%06X", (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF));

  sendFloatOtaActiveStatus();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(floatOtaSsid);
  floatOtaServer.on("/", HTTP_GET, []() {
    floatOtaServer.send(200, "text/html", floatOtaPageHtml());
  });
  floatOtaServer.on("/diag", HTTP_GET, []() {
    floatOtaServer.send(200, "text/html", floatOtaPageHtml());
  });
  floatOtaServer.on("/logo.png", HTTP_GET, []() {
    floatOtaServer.send_P(200, "image/png", (const char*)SGNODE_LOGO_PNG, SGNODE_LOGO_PNG_SIZE);
  });
  floatOtaServer.on("/reboot", HTTP_GET, []() {
    floatOtaServer.send(200, "text/html", "<html><body>Rebooting...</body></html>");
    delay(100);
    stopFloatOtaAndReboot();
  });
  floatOtaServer.on("/update", HTTP_POST, []() {
    bool ok = !Update.hasError();
    floatOtaServer.send(ok ? 200 : 500, "text/html", ok ? "<html><body>OK. Rebooting...</body></html>" : "<html><body>Update failed.</body></html>");
    delay(250);
    if (ok) stopFloatOtaAndReboot();
  }, []() {
    HTTPUpload& upload = floatOtaServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      floatOtaUploadInProgress = true;
      Update.begin(UPDATE_SIZE_UNKNOWN);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      Update.end(true);
      floatOtaUploadInProgress = false;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.end();
      floatOtaUploadInProgress = false;
    }
  });
  floatOtaServer.begin();
}

void stopFloatOtaAndReboot() {
  floatOtaServer.stop();
  WiFi.softAPdisconnect(true);
  delay(100);
  ESP.restart();
}

void handleFloatOtaMode() {
  floatOtaServer.handleClient();
  if (millis() - floatOtaStartedMs > OTA_TIMEOUT_MS && !floatOtaUploadInProgress) {
    stopFloatOtaAndReboot();
  }

  bool pressed = digitalRead(CALIBRATION_SWITCH_PIN) == LOW;
  if (pressed && !floatOtaButtonWasPressed) {
    floatOtaButtonWasPressed = true;
    floatOtaButtonPressMs = millis();
  } else if (!pressed) {
    floatOtaButtonWasPressed = false;
  } else if (millis() - floatOtaButtonPressMs >= OTA_LONG_PRESS_MS) {
    stopFloatOtaAndReboot();
  }

  if (millis() - lastLedToggle > 250UL) {
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    lastLedToggle = millis();
  }
}

// BMI160 address (confirmed by scanner)
#define BMI160_ADDR 0x69

void initIMU() {
  // BMI160 - using library approach like sensor_test.ino
  if (debug_mode) Serial.println("Initializing BMI160...");
  
  if (BMI160.begin(BMI160GenClass::I2C_MODE, Wire, 0x69)) {
    if (debug_mode) Serial.println("BMI160 initialized successfully");
    BMI160.setAccelerometerRange(2);  // 2G range
    BMI160.setAccelerometerRate(25);  // 25Hz
    if (debug_mode) Serial.println("BMI160 configured");
  } else {
    if (debug_mode) Serial.println("BMI160 initialization FAILED");
    sensorData.flags |= 0x02;  // Set sensor error flag
  }
  delay(50);
}

void suspendBMI160() {
  // Put BMI160 into suspend mode for power savings
  // In suspend mode, BMI160 draws ~3uA vs ~925uA in normal mode
  Wire.beginTransmission(0x69);
  Wire.write(0x7E);  // Command register address
  Wire.write(0x10);  // Suspend accelerometer
  Wire.endTransmission();
  
  delay(10);
  
  Wire.beginTransmission(0x69);
  Wire.write(0x7E);  // Command register address
  Wire.write(0x14);  // Suspend gyroscope
  Wire.endTransmission();
  
  if (debug_mode) Serial.println("BMI160 put into suspend mode");
}

void initTemperatureSensor() {
  if (debug_mode) Serial.println("Temperature source: BMI160 chip temperature");
  delay(50);
}

void initESPNow() {
  // Initialize WiFi in station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // Ensure we're not connected to any AP
  delay(100);
  
  if (debug_mode) {
    Serial.printf("WiFi MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                  WiFi.macAddress()[0], WiFi.macAddress()[1], WiFi.macAddress()[2],
                  WiFi.macAddress()[3], WiFi.macAddress()[4], WiFi.macAddress()[5]);
  }
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    if (debug_mode) Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Set up peer info
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, baseStationMac, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;  // Explicitly set interface
  
  if (debug_mode) {
    Serial.printf("Adding peer: %02X:%02X:%02X:%02X:%02X:%02X on channel %d\n",
                  baseStationMac[0], baseStationMac[1], baseStationMac[2],
                  baseStationMac[3], baseStationMac[4], baseStationMac[5], ESPNOW_CHANNEL);
  }
  
  // Add peer
  esp_err_t addResult = esp_now_add_peer(&peerInfo);
  if (addResult != ESP_OK) {
    if (debug_mode) {
      Serial.printf("Failed to add peer, error: %d\n", addResult);
      if (addResult == ESP_ERR_ESPNOW_NOT_INIT) {
        if (debug_mode) Serial.println("  ESP-NOW not initialized");
      }
      if (addResult == ESP_ERR_ESPNOW_IF) {
        if (debug_mode) Serial.println("  Invalid WiFi interface");
      }
      if (addResult == ESP_ERR_INVALID_ARG) {
        if (debug_mode) Serial.println("  Invalid argument");
      }
      if (addResult == ESP_ERR_ESPNOW_NO_MEM) {
        if (debug_mode) Serial.println("  Out of memory");
      }
      if (addResult == ESP_ERR_ESPNOW_FULL) {
        if (debug_mode) Serial.println("  Peer list full");
      }
      if (addResult == ESP_ERR_ESPNOW_EXIST) {
        if (debug_mode) Serial.println("  Peer already exists");
      }
    }
    return;
  }
  
  if (calibrationMode) {
    // Register receive callback (new ESP-IDF signature)
    esp_now_register_recv_cb([](const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
      onCalibrationCommand(recv_info->src_addr, data, len);
    });
    if (debug_mode) Serial.println("ESP-NOW initialized with calibration support");
  } else {
    esp_now_register_recv_cb([](const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
      onDataAck(recv_info->src_addr, data, len);
    });
    if (debug_mode) Serial.println("ESP-NOW initialized (transmit only)");
  }
}

void configureUnusedGPIOs() {
  // Configure unused GPIOs as OUTPUT LOW to minimize power consumption
  // Used GPIOs: 1/3(UART0), 12(switch), 15(battery), 16(LED), 26(I2C_SDA), 27(I2C_SCL)
  // Skip: UART pins (1, 3), strapping pins (0, 2, 15), FLASH pins (6-11), input-only pins (34-39), non-existent pins
  
  const uint8_t unusedGpios[] = {
    13, 14,         // Safe unused (SPI/JTAG)
    17, 18, 19, 20, // Safe unused (USB)
    21, 22,         // Safe unused (I2C master)
    23, 25,         // Safe unused (SPI/RTC)
    32, 33          // Safe unused (RTC only)
  };
  
  for (int i = 0; i < sizeof(unusedGpios) / sizeof(unusedGpios[0]); i++) {
    uint8_t pin = unusedGpios[i];
    // Only configure if pin is valid (0-33)
    if (pin <= 33) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
  }
  
  if (debug_mode) Serial.println("Unused GPIOs configured as OUTPUT LOW for power saving");
}

void ensureDebugSerial() {
  if (!Serial) {
    Serial.begin(115200);
    delay(50);
  }
}

float measureTilt() {
  // Read from BMI160 using library
  int ax, ay, az;
  BMI160.readAccelerometer(ax, ay, az);
  
  if (ax == 0 && ay == 0 && az == 0) {
    if (debug_mode) Serial.println("BMI160 reading zeros - possible error");
    return 45.0; // Fallback
  }
  
  // Apply sensor offsets if available
  bool offsetsApplied = applySensorOffsets(ax, ay, az);
  
  // Convert to tilt angle (deviation from vertical) - clean approach
  float ax_f = ax / 16384.0;
  float ay_f = ay / 16384.0;
  float az_f = az / 16384.0;
  
  // For Y-axis pointing down, use absolute value for tilt calculation
  ay_f = fabs(ay_f);
  
  float magnitude = sqrt(ax_f*ax_f + ay_f*ay_f + az_f*az_f);
  float angle_rad = acos(ay_f / magnitude);  // Angle from Y-axis
  float angle_deg = angle_rad * 180.0 / PI;
  
  // Only show tilt data if NOT in calibration mode
  if (!calibrationMode) {
    Serial.printf("Tilt: %.2f°\n", angle_deg);
    if (debug_mode) {
      Serial.printf("  Raw: ax=%d, ay=%d, az=%d\n", ax, ay, az);
      if (offsetsApplied) {
        Serial.printf("  Offsets applied: X=%.1f, Y=%.1f, Z=%.1f\n", 
                     sensorOffsets.offsetX, sensorOffsets.offsetY, sensorOffsets.offsetZ);
      }
    }
  }
  
  return angle_deg;
}

float measureImuTemperature() {
  int16_t raw = BMI160.getTemperature();
  if (raw == (int16_t)0x8000) {
    return NAN;
  }
  return 23.0f + ((float)raw / 512.0f);
}

float measureTemperature() {
  float temperature = measureImuTemperature();
  
  if (isnan(temperature) || isinf(temperature)) {
    if (debug_mode) Serial.println("Failed to read temperature from BMI160");
    sensorData.flags |= 0x02;
    return ema_temp;
  }
  
  if (temperature < -10.0 || temperature > 80.0) {
    if (debug_mode) Serial.printf("Temperature out of range: %.2f°C\n", temperature);
    sensorData.flags |= 0x02;
    return ema_temp;
  }
  
  // Apply EMA filter
  ema_temp = ema(ema_temp, temperature, EMA_ALPHA);
  
  // Only show temperature if NOT in calibration mode
  if (!calibrationMode) {
    Serial.printf("Temperature: %.2f°C\n", ema_temp);
    if (debug_mode) {
      Serial.printf("  Raw: %.2f°C\n", temperature);
    }
  }
  
  return ema_temp;
}

float calculateDensity(float angle, float temperature) {
  float sg = calculateGravity(angle);
  
  const float referenceTemp = 20.0;
  const float tempCorrectionCoeff = -0.0003;
  float tempCorrection = tempCorrectionCoeff * (temperature - referenceTemp);
  float correctedSG = sg + tempCorrection;
  
  if (correctedSG < 0.9) correctedSG = 0.9;
  if (correctedSG > 1.2) correctedSG = 1.2;
  
  // Only show density if NOT in calibration mode
  if (!calibrationMode) {
    Serial.printf("Density: %.4f SG\n", correctedSG);
    if (debug_mode) {
      Serial.printf("  Raw: SG=%.4f, Temp=%.2f°C\n", sg, temperature);
    }
  }

  return correctedSG;
}

uint8_t calculateSOC(float voltage) {
  // Li-ion battery SOC calculation (adjusted for 4.175V full charge)
  // Voltage points: 3.0V = 0%, 3.3V = 10%, 3.6V = 40%, 3.7V = 60%, 3.8V = 80%, 4.0V = 95%, 4.175V = 100%
  
  if (voltage <= 3.0) return 0;
  if (voltage >= 4.175) return 100;
  
  uint8_t soc;
  if (voltage <= 3.3) {
    soc = (uint8_t)((voltage - 3.0) / 0.3 * 10);  // 3.0V-3.3V: 0%-10%
  } else if (voltage <= 3.6) {
    soc = 10 + (uint8_t)((voltage - 3.3) / 0.3 * 30);  // 3.3V-3.6V: 10%-40%
  } else if (voltage <= 3.7) {
    soc = 40 + (uint8_t)((voltage - 3.6) / 0.1 * 20);  // 3.6V-3.7V: 40%-60%
  } else if (voltage <= 3.8) {
    soc = 60 + (uint8_t)((voltage - 3.7) / 0.1 * 20);  // 3.7V-3.8V: 60%-80%
  } else if (voltage <= 4.0) {
    soc = 80 + (uint8_t)((voltage - 3.8) / 0.2 * 15);  // 3.8V-4.0V: 80%-95%
  } else {
    soc = 95 + (uint8_t)((voltage - 4.0) / 0.175 * 5);  // 4.0V-4.175V: 95%-100%
  }
  
  // Cap at 100%
  return (soc > 100) ? 100 : soc;
}

float getBatteryVoltage() {
  // Read ADC value with averaging for better accuracy and power efficiency
  // Reduced from 10 samples to 5 samples with 5ms delays (25ms total vs 100ms)
  const int numSamples = 5;
  int total = 0;
  
  for (int i = 0; i < numSamples; i++) {
    total += analogRead(BATTERY_PIN);
    delay(5);  // 5ms delay between samples
  }
  
  int adc_value = total / numSamples;
  
  // Always show raw ADC value for battery debugging
  Serial.printf("Raw ADC pin 15 (avg of %d): %d", numSamples, adc_value);
  if (adc_value == 0) {
    Serial.println(" - No battery detected");
  } else {
    Serial.printf(" - Voltage: %.2fV\n", adc_value * 3.3 / 4095.0 * 2.0);
  }
  
  // If ADC is reading 0, there might be no battery connected or wrong pin
  if (adc_value == 0) {
    if (debug_mode) {
      Serial.println("ADC reading 0 - check battery connection to pin 15");
      Serial.println("Pin 15 should be connected to battery via voltage divider");
    }
    return 0.0;
  }
  
  // ESP32 ADC conversion: 0-4095 corresponds to 0-3.3V (with 11dB attenuation)
  // With 2:1 voltage divider, actual battery voltage = pin_voltage * 2
  float pin_voltage = adc_value * 3.3 / 4095.0;
  float voltage = pin_voltage * 2.0;  // 2:1 voltage divider ratio
  
  // Apply calibration factor - corrected for 13% measurement error
  // New factor 1.048 = (4.062V ÷ 3.54V) × 0.913
  const float voltage_calibration = 1.048;
  voltage *= voltage_calibration;
  
  // Sanity check - battery voltage should be reasonable
  if (voltage < 0.1) {
    if (debug_mode) Serial.printf("Battery voltage too low: %.2fV (ADC: %d)\n", voltage, adc_value);
    voltage = 0.0;  // Set to 0 if reading is invalid
  } else if (voltage > 5.0) {
    if (debug_mode) Serial.printf("Battery voltage too high: %.2fV (ADC: %d)\n", voltage, adc_value);
    voltage = 5.0;  // Cap at maximum reasonable voltage
  }
  
  // Check voltage thresholds and set flags
  if (voltage < 3.3 && voltage > 0.1) {
    sensorData.flags |= 0x04;  // Set low battery flag
  }
  
  // Calculate and show SOC
  uint8_t soc = calculateSOC(voltage);
  Serial.printf("Battery: %.2fV (%d%%)\n", voltage, soc);
  if (debug_mode) {
    Serial.printf("  ADC: %d, Raw: %.3fV\n", adc_value, adc_value * 3.3 / 4095.0);
  }
  
  return voltage;
}

void computeSensorData() {
  #if SGNODE_FLOAT_TEST_HARNESS
  if (harnessMockEnabled) {
    sensorData.angle = harnessMockAngle;
    sensorData.temperature = harnessMockTemp;
    sensorData.density = harnessMockSG;
    sensorData.uptime_s = (millis() / 1000) - boot_time;
    sequence_counter = rtcSequenceCounter;
    sensorData.sequence_id = sequence_counter++;
    rtcSequenceCounter = sequence_counter;
    sensorData.version = SG_PROTOCOL_VERSION;
    sensorData.flags = rtcPendingPayloadFlags;
    rtcPendingPayloadFlags = 0;
    sensorData.battery_voltage = harnessMockBattery;
    return;
  }
  #endif

  sensorData.temperature = measureTemperature();
  sensorData.density = calculateDensity(sensorData.angle, sensorData.temperature);
  sensorData.uptime_s = (millis() / 1000) - boot_time;
  sequence_counter = rtcSequenceCounter;
  sensorData.sequence_id = sequence_counter++;
  rtcSequenceCounter = sequence_counter;
  sensorData.version = SG_PROTOCOL_VERSION;
  sensorData.flags = rtcPendingPayloadFlags;
  rtcPendingPayloadFlags = 0;
  
  // Use cached battery voltage read before WiFi was enabled
  sensorData.battery_voltage = cachedBatteryVoltage;
  
  // Calculate SOC for display (not transmitted)
  uint8_t soc = calculateSOC(cachedBatteryVoltage);
  
  // Only show computed results if NOT in calibration mode
  if (!calibrationMode) {
    Serial.printf("Result: SG=%.4f, Temp=%.1f°C, Batt=%.2fV (%d%%), Seq=%d\n",
                  sensorData.density, sensorData.temperature, 
                  sensorData.battery_voltage, soc, sensorData.sequence_id);
  }
}

void transmitData() {
  if (debug_mode) Serial.println("Transmitting data...");

  storePayloadForRetry(sensorData);
  sendRetryPayloads();
  processQueuedZeroCalibrationCommand();
  attachPendingFlagsToPayload(&sensorData, true);
  bool acked = sendPayloadWithAck(&sensorData, true);
  processQueuedZeroCalibrationCommand();

  if (acked) {
    markAcked(sensorData.sequence_id);
  } else {
    sensorData.flags |= 0x01;  // Set delayed flag for local sleep interval handling
    storePayloadForRetry(sensorData);
  }

  // If the zero command arrived in the ACK for the current payload, the
  // current payload is already gone. Send the confirmation before sleeping.
  processQueuedZeroCalibrationCommand();
  sendPendingZeroCalibrationResult();

  delay(50);

  // Turn off WiFi completely (only in normal mode)
  if (!calibrationMode) {
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);  // Turn off WiFi radio
    delay(100);  // Wait for WiFi to fully shut down
  }
}

void onDataAck(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(ack_packet_t)) return;

  ack_packet_t ack;
  memcpy(&ack, incomingData, len);
  if (ack.packet_type != ACK_PACKET_TYPE) return;

  if (ack.command == SG_ACK_COMMAND_ZERO_CALIBRATE) {
    queuedZeroCalCommandId = ack.command_id;
    queuedZeroCalCommand = true;
  }

  receivedAckSeq = ack.sequence_id;
  markAcked(ack.sequence_id);
  if (ack.highest_seen > lastAckedSeq) {
    lastAckedSeq = ack.highest_seen;
  }
  ackReceived = true;
}

bool runQuickZeroCalibration() {
  const int sampleCount = 5;
  const int maxAxisSpan = 150;
  long totalX = 0;
  long totalY = 0;
  long totalZ = 0;
  int minX = 32767, minY = 32767, minZ = 32767;
  int maxX = -32768, maxY = -32768, maxZ = -32768;

  for (int i = 0; i < sampleCount; i++) {
    int ax, ay, az;
    BMI160.readAccelerometer(ax, ay, az);
    totalX += ax;
    totalY += ay;
    totalZ += az;
    if (ax < minX) minX = ax;
    if (ay < minY) minY = ay;
    if (az < minZ) minZ = az;
    if (ax > maxX) maxX = ax;
    if (ay > maxY) maxY = ay;
    if (az > maxZ) maxZ = az;
    delay(40);
  }

  float avgX = totalX / (float)sampleCount;
  float avgY = totalY / (float)sampleCount;
  float avgZ = totalZ / (float)sampleCount;
  float magnitude = sqrtf(avgX * avgX + avgY * avgY + avgZ * avgZ) / 16384.0f;
  bool stable = (maxX - minX) <= maxAxisSpan &&
                (maxY - minY) <= maxAxisSpan &&
                (maxZ - minZ) <= maxAxisSpan;
  bool plausible = magnitude >= 0.80f && magnitude <= 1.20f;
  if (!stable || !plausible) {
    if (debug_mode) {
      Serial.printf("Quick zero rejected stable=%d mag=%.3f span=%d/%d/%d\n",
                    stable ? 1 : 0, magnitude, maxX - minX, maxY - minY, maxZ - minZ);
    }
    return false;
  }

  float newOffsetX = avgX;
  float newOffsetY = avgY + 16384.0f;
  float newOffsetZ = avgZ;
  if (sensorOffsets.isValid) {
    float dx = newOffsetX - sensorOffsets.offsetX;
    float dy = newOffsetY - sensorOffsets.offsetY;
    float dz = newOffsetZ - sensorOffsets.offsetZ;
    float offsetDelta = sqrtf(dx * dx + dy * dy + dz * dz);
    if (offsetDelta > 2500.0f) {
      if (debug_mode) {
        Serial.printf("Quick zero rejected offset jump %.0f\n", offsetDelta);
      }
      return false;
    }
  }

  sensorOffsets.offsetX = newOffsetX;
  sensorOffsets.offsetY = newOffsetY;
  sensorOffsets.offsetZ = newOffsetZ;
  sensorOffsets.isValid = true;
  return saveSensorOffsets();
}

bool processQueuedZeroCalibrationCommand() {
  if (!queuedZeroCalCommand) return false;

  uint8_t commandId = queuedZeroCalCommandId;
  queuedZeroCalCommand = false;
  if (zeroCalProcessedThisWake) return false;

  bool ok = runQuickZeroCalibration();
  zeroCalProcessedThisWake = true;
  rtcLastAckCommandId = commandId;
  rtcPendingPayloadFlags &= ~(SG_PAYLOAD_FLAG_ZERO_CAL_OK | SG_PAYLOAD_FLAG_ZERO_CAL_FAIL);
  rtcPendingPayloadFlags |= ok ? SG_PAYLOAD_FLAG_ZERO_CAL_OK : SG_PAYLOAD_FLAG_ZERO_CAL_FAIL;
  return true;
}

bool attachPendingFlagsToPayload(payload_t* payload, bool refreshMeasurement) {
  if (!payload || rtcPendingPayloadFlags == 0) return false;

  uint8_t pendingFlags = rtcPendingPayloadFlags;
  rtcPendingPayloadFlags = 0;
  if (refreshMeasurement) {
    payload->angle = measureTilt();
    payload->temperature = measureTemperature();
    payload->density = calculateDensity(payload->angle, payload->temperature);
    payload->uptime_s = (millis() / 1000) - boot_time;
  }
  payload->flags |= pendingFlags;
  return true;
}

bool sendPendingZeroCalibrationResult() {
  if ((rtcPendingPayloadFlags & (SG_PAYLOAD_FLAG_ZERO_CAL_OK | SG_PAYLOAD_FLAG_ZERO_CAL_FAIL)) == 0) {
    return false;
  }

  payload_t resultPayload = {};
  resultPayload.version = SG_PROTOCOL_VERSION;
  resultPayload.angle = measureTilt();
  resultPayload.temperature = measureTemperature();
  resultPayload.density = calculateDensity(resultPayload.angle, resultPayload.temperature);
  resultPayload.uptime_s = (millis() / 1000) - boot_time;
  sequence_counter = rtcSequenceCounter;
  resultPayload.sequence_id = sequence_counter++;
  rtcSequenceCounter = sequence_counter;
  resultPayload.battery_voltage = cachedBatteryVoltage;
  resultPayload.flags = rtcPendingPayloadFlags;
  rtcPendingPayloadFlags = 0;

  storePayloadForRetry(resultPayload);
  bool acked = sendPayloadWithAck(&resultPayload, true);
  if (acked) {
    markAcked(resultPayload.sequence_id);
  } else {
    resultPayload.flags |= SG_PAYLOAD_FLAG_DELAYED;
    storePayloadForRetry(resultPayload);
  }
  return acked;
}

void storePayloadForRetry(const payload_t& payload) {
  for (int i = 0; i < RETRY_BUFFER_SIZE; i++) {
    if (retryBuffer[i].valid && retryBuffer[i].payload.sequence_id == payload.sequence_id) {
      retryBuffer[i].payload = payload;
      return;
    }
  }

  retryBuffer[retryWriteIndex].valid = true;
  retryBuffer[retryWriteIndex].payload = payload;
  retryWriteIndex = (retryWriteIndex + 1) % RETRY_BUFFER_SIZE;
}

void markAcked(uint16_t sequence_id) {
  lastAckedSeq = sequence_id;
  for (int i = 0; i < RETRY_BUFFER_SIZE; i++) {
    if (retryBuffer[i].valid && retryBuffer[i].payload.sequence_id == sequence_id) {
      retryBuffer[i].valid = false;
    }
  }
}

bool sendPayloadWithAck(payload_t* payload, bool waitForAck) {
  size_t payload_size = sizeof(*payload) - sizeof(payload->crc) - sizeof(payload->version);
  payload->crc = crc16((const uint8_t*)payload + sizeof(payload->version), payload_size);

  esp_err_t result = ESP_FAIL;
  for (int i = 0; i <= MAX_RETRIES; i++) {
    ackReceived = false;
    receivedAckSeq = 0;
    result = esp_now_send(baseStationMac, (uint8_t *)payload, sizeof(*payload));

    if (result == ESP_OK) {
      if (debug_mode) Serial.println("Data sent successfully");
      break;
    }
    
    if (i < MAX_RETRIES) {
      if (debug_mode) Serial.printf("Retry %d...\n", i + 1);
      delay(100);
    }
  }

  if (result != ESP_OK) {
    if (debug_mode) {
      Serial.printf("Error sending data after retries: %d\n", result);
      if (result == ESP_ERR_ESPNOW_NOT_INIT) Serial.println("  ESP-NOW not initialized");
      if (result == ESP_ERR_ESPNOW_IF) Serial.println("  Invalid WiFi interface");
      if (result == ESP_ERR_INVALID_ARG) Serial.println("  Invalid argument");
      if (result == ESP_ERR_ESPNOW_NO_MEM) Serial.println("  Out of memory");
      if (result == ESP_ERR_ESPNOW_NOT_FOUND) Serial.println("  Peer not found");
      if (result == ESP_ERR_ESPNOW_IF) Serial.println("  WiFi interface not connected");
      Serial.printf("  Target MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    baseStationMac[0], baseStationMac[1], baseStationMac[2],
                    baseStationMac[3], baseStationMac[4], baseStationMac[5]);
    }
    return false;
  }

  if (!waitForAck) return false;

  unsigned long start = millis();
  while (millis() - start < ACK_WAIT_MS) {
    processQueuedZeroCalibrationCommand();
    if (ackReceived && receivedAckSeq == payload->sequence_id) {
      return true;
    }
    delay(5);
  }
  processQueuedZeroCalibrationCommand();
  return false;
}

void sendRetryPayloads(bool includeCurrent) {
  int sent = 0;
  for (int i = 0; i < RETRY_BUFFER_SIZE && sent < 2; i++) {
    if (!retryBuffer[i].valid) continue;
    if (!includeCurrent && retryBuffer[i].payload.sequence_id == sensorData.sequence_id) continue;

    retryBuffer[i].payload.flags |= 0x01; // delayed/retry flag
    if (sendPayloadWithAck(&retryBuffer[i].payload, true)) {
      markAcked(retryBuffer[i].payload.sequence_id);
    }
    sent++;
  }
}

void enterDeepSleep() {
  #if SGNODE_FLOAT_TEST_HARNESS
  if (harnessNoSleep) {
    if (debug_mode) Serial.println("Harness no_sleep active - staying awake");
    delay(1000);
    currentState = INIT;
    return;
  }
  #endif

  if (debug_mode) Serial.println("Entering deep sleep...");
  
  // Put BMI160 into suspend mode for power savings (~3uA vs ~925uA)
  suspendBMI160();
  
  // Ensure WiFi/ESP-NOW is fully deinitialized before sleep
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);  // Turn off WiFi radio
  
  // Adjust sleep interval based on battery voltage
  uint64_t sleep_duration = MEASUREMENT_INTERVAL * 1000000ULL;
  if (sensorData.flags & 0x04) {  // Low battery flag
    sleep_duration *= 2;  // Double interval at low voltage
    if (debug_mode) Serial.println("Low battery: doubling sleep interval");
  }
  
  // Configure GPIO interrupt for calibration switch (EXT0)
  // This allows waking from deepsleep when calibration switch is pressed
  esp_sleep_enable_ext0_wakeup((gpio_num_t)CALIBRATION_SWITCH_PIN, 0); // Wake on LOW level
  
  // Enable timer wakeup as backup
  esp_sleep_enable_timer_wakeup(sleep_duration);
  
  if (debug_mode) {
    Serial.printf("Sleep duration: %llu seconds\n", sleep_duration / 1000000ULL);
    Serial.println("Calibration switch interrupt enabled for wake-up");
  }
  
  // Flush Serial to ensure all debug output is sent before sleep
  if (debug_mode) Serial.flush();
  
  esp_deep_sleep_start();
}

// Remote calibration command handler (only active in calibration mode)
void onCalibrationCommand(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (!calibrationMode) {
    return;
  }

  ensureDebugSerial();
  debug_mode = true;
  
  // Update activity timestamp on any calibration command
  updateCalibrationActivity();
  
  if (len != sizeof(calib_command_t)) {
    if (debug_mode) Serial.println("Invalid calibration command size");
    return;
  }
  
  calib_command_t cmd;
  memcpy(&cmd, incomingData, len);
  
  if (debug_mode) Serial.printf("Received calibration command: %d, target_sg=%.3f, request_id=%d\n", 
                cmd.command, cmd.target_sg, cmd.request_id);
  
  switch (cmd.command) {
    case 5: // CALIBRATE_OFFSET
      if (debug_mode) Serial.println("Sensor offset calibration requested");
      {
        if (true) {
          startStableOffsetCalibration(cmd.request_id);
        } else if (calibrateSensorOffset()) {
          // Send response with the calculated offset angle (should be close to 0°)
          float offsetAngle = measureTilt(); // This should now be close to 0° with offsets applied
          sendCalibrationResponse(offsetAngle, 0.0, cmd.request_id, "Offset calibrated");
          
          if (debug_mode) {
            Serial.printf("Offset calibration completed: %.2f°\n", offsetAngle);
            printSensorOffsets();
          }
        } else {
          // Send error response
          sendCalibrationResponse(0.0, 0.0, cmd.request_id, "Offset calibration failed");
          if (debug_mode) Serial.println("Offset calibration failed");
        }
      }
      break;
      
    case 4: // APPLY_CALIBRATION
      if (debug_mode) Serial.println("Apply calibration requested");
      {
        // Calculate polynomial coefficients from calibration points
        if (calculatePolynomialCoefficients()) {
          // Save coefficients to EEPROM
          saveCalibrationCoefficients();
          
          // Send confirmation response
          sendCalibrationResponse(0.0, 0.0, cmd.request_id, "Calibration applied successfully");
          
          // Send coefficients to base station for graph display
          sendCalibrationCoefficients(cmd.request_id);
          
          if (debug_mode) {
            Serial.println("Polynomial coefficients calculated and saved");
            printCalibrationCoefficients();
          }
        } else {
          // Send error response
          sendCalibrationResponse(0.0, 0.0, cmd.request_id, "Failed to calculate coefficients");
          if (debug_mode) Serial.println("Failed to calculate polynomial coefficients");
        }
        
        // Exit calibration mode regardless of success/failure
        exitCalibrationMode();
      }
      break;
      
    case 7: // EXIT_CALIBRATION
      if (debug_mode) Serial.println("Exit calibration requested");
      {
        // Send confirmation response
        sendCalibrationResponse(0.0, 0.0, cmd.request_id, "Calibration mode exited");
        
        // Exit calibration mode
        exitCalibrationMode();
        
        if (debug_mode) Serial.println("Calibration mode exited (offset only saved)");
      }
      break;
      
    case 0: // CALIBRATE_POINT1 (Water SG=1.000)
      if (debug_mode) Serial.println("Point 1 calibration measurement requested (SG=1.000)");
      {
        // Take multiple readings and average
        float totalAngle = 0.0;
        float totalTemp = 0.0;
        int numReadings = 5;
        
        for (int i = 0; i < numReadings; i++) {
          totalAngle += measureTilt();
          totalTemp += measureTemperature();
          delay(100); // Small delay between readings
        }
        
        float avgAngle = totalAngle / numReadings;
        float avgTemp = totalTemp / numReadings;
        
        addCalibrationPoint(avgAngle, cmd.target_sg, avgTemp);
        sendCalibrationResponse(avgAngle, cmd.target_sg, cmd.request_id, "Point 1 added");
        
        if (debug_mode) {
          Serial.printf("Point 1 averaged: %.2f°, %.1f°C (%d readings)\n", avgAngle, avgTemp, numReadings);
        }
      }
      break;
      
    case 1: // CALIBRATE_POINT2 (Light Sugar SG=1.040)
      if (debug_mode) Serial.println("Point 2 calibration measurement requested (SG=1.040)");
      {
        // Take multiple readings and average
        float totalAngle = 0.0;
        float totalTemp = 0.0;
        int numReadings = 5;
        
        for (int i = 0; i < numReadings; i++) {
          totalAngle += measureTilt();
          totalTemp += measureTemperature();
          delay(100); // Small delay between readings
        }
        
        float avgAngle = totalAngle / numReadings;
        float avgTemp = totalTemp / numReadings;
        
        addCalibrationPoint(avgAngle, cmd.target_sg, avgTemp);
        sendCalibrationResponse(avgAngle, cmd.target_sg, cmd.request_id, "Point 2 added");
        
        if (debug_mode) {
          Serial.printf("Point 2 averaged: %.2f°, %.1f°C (%d readings)\n", avgAngle, avgTemp, numReadings);
        }
      }
      break;
      
    case 2: // CALIBRATE_POINT3 (Medium Sugar SG=1.080)
      if (debug_mode) Serial.println("Point 3 calibration measurement requested (SG=1.080)");
      {
        // Take multiple readings and average
        float totalAngle = 0.0;
        float totalTemp = 0.0;
        int numReadings = 5;
        
        for (int i = 0; i < numReadings; i++) {
          totalAngle += measureTilt();
          totalTemp += measureTemperature();
          delay(100); // Small delay between readings
        }
        
        float avgAngle = totalAngle / numReadings;
        float avgTemp = totalTemp / numReadings;
        
        addCalibrationPoint(avgAngle, cmd.target_sg, avgTemp);
        sendCalibrationResponse(avgAngle, cmd.target_sg, cmd.request_id, "Point 3 added");
        
        if (debug_mode) {
          Serial.printf("Point 3 averaged: %.2f°, %.1f°C (%d readings)\n", avgAngle, avgTemp, numReadings);
        }
      }
      break;
      
    case 3: // CALIBRATE_POINT4 (Heavy Sugar SG=1.120)
      if (debug_mode) Serial.println("Point 4 calibration measurement requested (SG=1.120)");
      {
        // Take multiple readings and average
        float totalAngle = 0.0;
        float totalTemp = 0.0;
        int numReadings = 5;
        
        for (int i = 0; i < numReadings; i++) {
          totalAngle += measureTilt();
          totalTemp += measureTemperature();
          delay(100); // Small delay between readings
        }
        
        float avgAngle = totalAngle / numReadings;
        float avgTemp = totalTemp / numReadings;
        
        addCalibrationPoint(avgAngle, cmd.target_sg, avgTemp);
        sendCalibrationResponse(avgAngle, cmd.target_sg, cmd.request_id, "Point 4 added");
        
        if (debug_mode) {
          Serial.printf("Point 4 averaged: %.2f°, %.1f°C (%d readings)\n", avgAngle, avgTemp, numReadings);
        }
      }
      break;

    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
      recordCalibrationPoint(cmd.target_sg, cmd.request_id, cmd.command - 7);
      break;
      
          
    default:
      if (debug_mode) Serial.println("Unknown calibration command");
      sendCalibrationResponse(0, 0, cmd.request_id, "Unknown command");
      break;
  }
}

void recordCalibrationPoint(float target_sg, uint8_t request_id, uint8_t point_number) {
  if (debug_mode) {
    Serial.printf("Point %u calibration measurement requested (SG=%.3f)\n", point_number, target_sg);
  }

  float totalAngle = 0.0f;
  float totalTemp = 0.0f;
  const int numReadings = 5;

  for (int i = 0; i < numReadings; i++) {
    totalAngle += measureTilt();
    totalTemp += measureTemperature();
    delay(100);
  }

  float avgAngle = totalAngle / numReadings;
  float avgTemp = totalTemp / numReadings;
  addCalibrationPoint(avgAngle, target_sg, avgTemp);

  char message[24];
  snprintf(message, sizeof(message), "Point %u added", point_number);
  sendCalibrationResponse(avgAngle, target_sg, request_id, message);

  if (debug_mode) {
    Serial.printf("Point %u averaged: %.2f deg, %.1f C (%d readings)\n",
                  point_number, avgAngle, avgTemp, numReadings);
  }
  updateCalibrationActivity();
}

void startStableOffsetCalibration(uint8_t request_id) {
  ensureDebugSerial();
  debug_mode = true;
  memset(&pendingStableOffset, 0, sizeof(pendingStableOffset));
  pendingStableOffset.active = true;
  pendingStableOffset.requestId = request_id;
  pendingStableOffset.startedMs = millis();
  pendingStableOffset.lastSampleMs = 0;
  pendingStableOffset.minX = pendingStableOffset.minY = pendingStableOffset.minZ = 32767;
  pendingStableOffset.maxX = pendingStableOffset.maxY = pendingStableOffset.maxZ = -32768;
  updateCalibrationActivity();
  Serial.printf("Offset stable calibration started request=%u\n", request_id);
}

void handlePendingStableOffsetCalibration() {
  if (!pendingStableOffset.active) return;

  const unsigned long minStableMs = 60000UL;
  const unsigned long maxStableMs = 600000UL;
  const unsigned long sampleIntervalMs = 10000UL;
  const float maxAxisDriftPerMin = 150.0f;
  const float maxTempDriftPerMin = 0.20f;
  const int maxShortSpan = 1200;
  unsigned long now = millis();
  if (pendingStableOffset.lastSampleMs != 0 && now - pendingStableOffset.lastSampleMs < sampleIntervalMs) {
    return;
  }

  pendingStableOffset.lastSampleMs = now;
  int ax, ay, az;
  BMI160.readAccelerometer(ax, ay, az);
  float temp = measureImuTemperature();
  if (isnan(temp) || isinf(temp)) {
    temp = measureTemperature();
  }
  if (pendingStableOffset.sampleCount == 0) {
    pendingStableOffset.firstX = ax;
    pendingStableOffset.firstY = ay;
    pendingStableOffset.firstZ = az;
    pendingStableOffset.firstTemp = temp;
  }
  pendingStableOffset.lastX = ax;
  pendingStableOffset.lastY = ay;
  pendingStableOffset.lastZ = az;
  pendingStableOffset.lastTemp = temp;
  pendingStableOffset.sumX += ax;
  pendingStableOffset.sumY += ay;
  pendingStableOffset.sumZ += az;
  pendingStableOffset.sumTemp += temp;
  pendingStableOffset.sampleCount++;
  if (ax < pendingStableOffset.minX) pendingStableOffset.minX = ax;
  if (ay < pendingStableOffset.minY) pendingStableOffset.minY = ay;
  if (az < pendingStableOffset.minZ) pendingStableOffset.minZ = az;
  if (ax > pendingStableOffset.maxX) pendingStableOffset.maxX = ax;
  if (ay > pendingStableOffset.maxY) pendingStableOffset.maxY = ay;
  if (az > pendingStableOffset.maxZ) pendingStableOffset.maxZ = az;
  updateCalibrationActivity();

  unsigned long elapsedMs = now - pendingStableOffset.startedMs;
  float elapsedMin = elapsedMs / 60000.0f;
  if (elapsedMin < 0.1f) elapsedMin = 0.1f;
  float driftX = fabsf(pendingStableOffset.lastX - pendingStableOffset.firstX) / elapsedMin;
  float driftY = fabsf(pendingStableOffset.lastY - pendingStableOffset.firstY) / elapsedMin;
  float driftZ = fabsf(pendingStableOffset.lastZ - pendingStableOffset.firstZ) / elapsedMin;
  float tempDrift = fabsf(pendingStableOffset.lastTemp - pendingStableOffset.firstTemp) / elapsedMin;
  if (tempDrift > pendingStableOffset.peakTempDrift) {
    pendingStableOffset.peakTempDrift = tempDrift;
  }
  float avgX = pendingStableOffset.sumX / pendingStableOffset.sampleCount;
  float avgY = pendingStableOffset.sumY / pendingStableOffset.sampleCount;
  float avgZ = pendingStableOffset.sumZ / pendingStableOffset.sampleCount;
  float magnitude = sqrtf(avgX * avgX + avgY * avgY + avgZ * avgZ) / 16384.0f;
  bool vectorValid = magnitude >= 0.80f && magnitude <= 1.20f;
  bool stable = elapsedMs >= minStableMs &&
                pendingStableOffset.sampleCount >= 6 &&
                driftX <= maxAxisDriftPerMin &&
                driftY <= maxAxisDriftPerMin &&
                driftZ <= maxAxisDriftPerMin &&
                vectorValid &&
                (pendingStableOffset.maxX - pendingStableOffset.minX) <= maxShortSpan &&
                (pendingStableOffset.maxY - pendingStableOffset.minY) <= maxShortSpan &&
                (pendingStableOffset.maxZ - pendingStableOffset.minZ) <= maxShortSpan;
  bool timeout = elapsedMs >= maxStableMs;
  float minRemainingSec = elapsedMs < minStableMs ? (minStableMs - elapsedMs) / 1000.0f : 0.0f;
  float tempRemainingSec = tempDrift > maxTempDriftPerMin ? 120.0f : 0.0f;
  if (tempDrift > maxTempDriftPerMin &&
      pendingStableOffset.previousTempDrift > tempDrift &&
      pendingStableOffset.previousTempDriftMs > 0 &&
      now > pendingStableOffset.previousTempDriftMs) {
    float dtMin = (now - pendingStableOffset.previousTempDriftMs) / 60000.0f;
    float decay = logf(pendingStableOffset.previousTempDrift / tempDrift) / dtMin;
    if (decay > 0.001f && decay < 10.0f) {
      tempRemainingSec = logf(tempDrift / maxTempDriftPerMin) / decay * 60.0f;
      if (tempRemainingSec < 0.0f) tempRemainingSec = 0.0f;
    }
  }
  pendingStableOffset.previousTempDrift = tempDrift;
  pendingStableOffset.previousTempDriftMs = now;
  float maxRemainingSec = elapsedMs < maxStableMs ? (maxStableMs - elapsedMs) / 1000.0f : 0.0f;
  if (tempRemainingSec > maxRemainingSec) tempRemainingSec = maxRemainingSec;
  float etaSeconds = minRemainingSec > tempRemainingSec ? minRemainingSec : tempRemainingSec;
  if (!stable && !timeout) {
    if (!vectorValid) {
      char status[24];
      snprintf(status, sizeof(status), "Mag %.2fg", magnitude);
      sendCalibrationStatus(0.0f, tempDrift, pendingStableOffset.requestId, status);
    } else if (driftX > maxAxisDriftPerMin || driftY > maxAxisDriftPerMin || driftZ > maxAxisDriftPerMin ||
               (pendingStableOffset.maxX - pendingStableOffset.minX) > maxShortSpan ||
               (pendingStableOffset.maxY - pendingStableOffset.minY) > maxShortSpan ||
               (pendingStableOffset.maxZ - pendingStableOffset.minZ) > maxShortSpan) {
      sendCalibrationStatus(etaSeconds, tempDrift, pendingStableOffset.requestId, "Hold still");
    } else if (tempDrift > maxTempDriftPerMin) {
      sendCalibrationStatus(etaSeconds, tempDrift, pendingStableOffset.requestId, "G stable soon");
    } else {
      sendCalibrationStatus(etaSeconds, tempDrift, pendingStableOffset.requestId, "G stable soon");
    }
    Serial.printf("Offset sample avg=%0.f/%0.f/%0.f mag=%.3f drift=%0.f/%0.f/%0.f span=%d/%d/%d temp=%.2f rate=%.3f samples=%d\n",
                  avgX, avgY, avgZ, magnitude, driftX, driftY, driftZ,
                  pendingStableOffset.maxX - pendingStableOffset.minX,
                  pendingStableOffset.maxY - pendingStableOffset.minY,
                  pendingStableOffset.maxZ - pendingStableOffset.minZ,
                  pendingStableOffset.lastTemp, tempDrift, pendingStableOffset.sampleCount);
    return;
  }

  if (!stable && !vectorValid) {
    pendingStableOffset.active = false;
    char message[32];
    snprintf(message, sizeof(message), "Offset mag %.2fg", magnitude);
    sendCalibrationResponse(0.0f, 0.0f, pendingStableOffset.requestId, message);
    if (debug_mode) {
      Serial.printf("Offset calibration failed: mag=%.3f avg=%0.f/%0.f/%0.f span=%d/%d/%d\n",
                    magnitude, avgX, avgY, avgZ,
                    pendingStableOffset.maxX - pendingStableOffset.minX,
                    pendingStableOffset.maxY - pendingStableOffset.minY,
                    pendingStableOffset.maxZ - pendingStableOffset.minZ);
    }
    return;
  }

  float newOffsetX = avgX;
  float newOffsetY = avgY + 16384.0f;
  float newOffsetZ = avgZ;
  if (sensorOffsets.isValid) {
    float dx = newOffsetX - sensorOffsets.offsetX;
    float dy = newOffsetY - sensorOffsets.offsetY;
    float dz = newOffsetZ - sensorOffsets.offsetZ;
    float offsetDelta = sqrtf(dx * dx + dy * dy + dz * dz);
    if (offsetDelta > 2500.0f) {
      pendingStableOffset.active = false;
      char message[32];
      snprintf(message, sizeof(message), "Offset jump %.0f", offsetDelta);
      sendCalibrationResponse(0.0f, 0.0f, pendingStableOffset.requestId, message);
      if (debug_mode) {
        Serial.printf("Offset calibration rejected: delta=%.0f old=%.0f/%.0f/%.0f new=%.0f/%.0f/%.0f\n",
                      offsetDelta, sensorOffsets.offsetX, sensorOffsets.offsetY, sensorOffsets.offsetZ,
                      newOffsetX, newOffsetY, newOffsetZ);
      }
      return;
    }
  }

  sensorOffsets.offsetX = newOffsetX;
  sensorOffsets.offsetY = newOffsetY;
  sensorOffsets.offsetZ = newOffsetZ;
  sensorOffsets.isValid = true;
  bool saved = saveSensorOffsets();
  float offsetAngle = saved ? measureTilt() : 0.0f;
  pendingStableOffset.active = false;
  sendCalibrationResponse(offsetAngle, 0.0f, pendingStableOffset.requestId,
                          saved ? (stable ? "Offset stable" : "Offset max wait") : "Offset save failed");
  if (debug_mode) {
    Serial.printf("Offset calibration %s: angle %.2f samples=%d mag=%.3f\n",
                  saved ? "saved" : "failed", offsetAngle,
                  pendingStableOffset.sampleCount, magnitude);
  }
}

void handlePendingStableCalibrationPoint() {
  if (!pendingStablePoint.active) return;

  const unsigned long minStableMs = 120000UL;
  const unsigned long maxStableMs = 600000UL;
  const unsigned long sampleIntervalMs = 10000UL;
  unsigned long now = millis();
  if (pendingStablePoint.lastSampleMs != 0 && now - pendingStablePoint.lastSampleMs < sampleIntervalMs) {
    return;
  }

  pendingStablePoint.lastSampleMs = now;
  float angle = measureTilt();
  float temp = measureTemperature();
  float sg = calculateDensity(angle, temp);
  if (pendingStablePoint.sampleCount == 0) {
    pendingStablePoint.firstAngle = angle;
    pendingStablePoint.firstTemp = temp;
    pendingStablePoint.firstSG = sg;
  }
  pendingStablePoint.lastAngle = angle;
  pendingStablePoint.lastTemp = temp;
  pendingStablePoint.lastSG = sg;
  pendingStablePoint.sumAngle += angle;
  pendingStablePoint.sumTemp += temp;
  pendingStablePoint.sampleCount++;
  updateCalibrationActivity();

  unsigned long elapsedMs = now - pendingStablePoint.startedMs;
  float elapsedMin = elapsedMs / 60000.0f;
  if (elapsedMin < 0.1f) elapsedMin = 0.1f;
  float angleDriftPerMin = fabsf(pendingStablePoint.lastAngle - pendingStablePoint.firstAngle) / elapsedMin;
  float tempDriftPerMin = fabsf(pendingStablePoint.lastTemp - pendingStablePoint.firstTemp) / elapsedMin;
  float sgDriftPerMin = fabsf(pendingStablePoint.lastSG - pendingStablePoint.firstSG) / elapsedMin;
  bool stable = elapsedMs >= minStableMs &&
                pendingStablePoint.sampleCount >= 6 &&
                angleDriftPerMin <= 0.05f &&
                tempDriftPerMin <= 0.05f &&
                sgDriftPerMin <= 0.0002f;
  bool timeout = elapsedMs >= maxStableMs;

  if (!stable && !timeout) {
    if (debug_mode) {
      Serial.printf("Point %u stabilizing: angle %.2f/min temp %.3f/min sg %.5f/min samples=%d\n",
                    pendingStablePoint.pointNumber, angleDriftPerMin, tempDriftPerMin,
                    sgDriftPerMin, pendingStablePoint.sampleCount);
    }
    return;
  }

  float avgAngle = pendingStablePoint.sumAngle / pendingStablePoint.sampleCount;
  float avgTemp = pendingStablePoint.sumTemp / pendingStablePoint.sampleCount;
  addCalibrationPoint(avgAngle, pendingStablePoint.targetSG, avgTemp);

  char message[24];
  snprintf(message, sizeof(message), timeout ? "Point %u max wait" : "Point %u stable", pendingStablePoint.pointNumber);
  sendCalibrationResponse(avgAngle, pendingStablePoint.targetSG, pendingStablePoint.requestId, message);

  if (debug_mode) {
    Serial.printf("Point %u accepted: %.2f deg, %.1f C (%d samples, stable=%d)\n",
                  pendingStablePoint.pointNumber, avgAngle, avgTemp,
                  pendingStablePoint.sampleCount, stable ? 1 : 0);
  }
  pendingStablePoint.active = false;
}

void sendCalibrationCoefficients(uint8_t request_id) {
  calib_coeffs_t coeffs;
  coeffs.response_type = 3; // COEFFICIENTS
  coeffs.coeff3 = calibCoeffs.coeff3;
  coeffs.coeff2 = calibCoeffs.coeff2;
  coeffs.coeff1 = calibCoeffs.coeff1;
  coeffs.coeff0 = calibCoeffs.coeff0;
  
  // Calculate normalization parameters from calibration points
  if (numCalibPoints >= 2) {
    coeffs.norm_offset = calibPoints[0].tilt; // First point as offset
    coeffs.norm_scale = calibPoints[numCalibPoints-1].tilt - calibPoints[0].tilt; // Range
  } else {
    coeffs.norm_offset = 0.0;
    coeffs.norm_scale = 1.0;
  }
  
  coeffs.request_id = request_id;
  
  // Send to base station
  esp_err_t result = esp_now_send(baseStationMac, (uint8_t*)&coeffs, sizeof(coeffs));
  if (result == ESP_OK) {
    if (debug_mode) {
      Serial.println("Calibration coefficients sent to base station");
      Serial.printf("Normalization: norm(Tilt) = (Tilt - %.2f) / %.2f\n", coeffs.norm_offset, coeffs.norm_scale);
    }
  } else {
    if (debug_mode) Serial.printf("Failed to send coefficients: %d\n", result);
  }
}

void sendCalibrationResponse(float angle, float target_sg, uint8_t request_id, const char* message) {
  // Rate limiting: only send calibration response if enough time has passed
  unsigned long currentTime = millis();
  if (currentTime - lastCalibrationResponse < CALIBRATION_RESPONSE_INTERVAL) {
    if (debug_mode) Serial.printf("Calibration response rate limited (wait %dms more)\n", 
                  CALIBRATION_RESPONSE_INTERVAL - (currentTime - lastCalibrationResponse));
    calibrationResponsePending = true; // Mark as pending for later transmission
    return;
  }
  
  calib_response_t response;
  response.response_type = 0; // CALIBRATION_DATA
  response.angle = angle;
  response.sg = target_sg;
  response.request_id = request_id;
  strncpy(response.message, message, sizeof(response.message) - 1);
  response.message[sizeof(response.message) - 1] = '\0';
  
  esp_err_t result = esp_now_send(baseStationMac, (uint8_t *)&response, sizeof(response));
  
  if (result == ESP_OK) {
    lastCalibrationResponse = currentTime;
    calibrationResponsePending = false;
    if (debug_mode) Serial.printf("Calibration response sent: angle=%.2f°, sg=%.3f, msg=%s\n", 
                  angle, target_sg, message);
  } else {
    if (debug_mode) Serial.printf("Error sending calibration response: %d\n", result);
    calibrationResponsePending = true; // Mark as pending for retry
  }
}

void sendCalibrationStatus(float etaSeconds, float tempDriftPerMin, uint8_t request_id, const char* message) {
  unsigned long currentTime = millis();
  if (currentTime - lastCalibrationResponse < CALIBRATION_RESPONSE_INTERVAL) {
    return;
  }

  calib_response_t response;
  response.response_type = 1;
  response.angle = tempDriftPerMin;
  response.sg = etaSeconds;
  response.request_id = request_id;
  strncpy(response.message, message, sizeof(response.message) - 1);
  response.message[sizeof(response.message) - 1] = '\0';

  esp_err_t result = esp_now_send(baseStationMac, (uint8_t *)&response, sizeof(response));
  if (result == ESP_OK) {
    lastCalibrationResponse = currentTime;
    if (debug_mode) {
      Serial.printf("Calibration status sent: eta=%.0fs tempDrift=%.3f msg=%s\n",
                    etaSeconds, tempDriftPerMin, message);
    }
  }
}

// CRC16 calculation for data integrity
uint16_t crc16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc >> 1) ^ (crc & 1 ? 0xA001 : 0);
    }
  }
  return crc;
}

#if SGNODE_FLOAT_TEST_HARNESS
void printFloatHarnessOK(const char* message) {
  Serial.print("OK");
  if (message && message[0]) {
    Serial.print(' ');
    Serial.print(message);
  }
  Serial.println();
}

void printFloatHarnessERR(const char* message) {
  Serial.print("ERR");
  if (message && message[0]) {
    Serial.print(' ');
    Serial.print(message);
  }
  Serial.println();
}

int retryBufferCount() {
  int count = 0;
  for (int i = 0; i < RETRY_BUFFER_SIZE; i++) {
    if (retryBuffer[i].valid) count++;
  }
  return count;
}

void handleFloatTestHarness() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      harnessCommandBuffer[harnessCommandLength] = '\0';
      if (harnessCommandLength > 0) processFloatHarnessCommand(harnessCommandBuffer);
      harnessCommandLength = 0;
    } else if (harnessCommandLength + 1 < sizeof(harnessCommandBuffer)) {
      harnessCommandBuffer[harnessCommandLength++] = c;
    }
  }
}

bool processFloatHarnessCommand(char* line) {
  char* args = strchr(line, ' ');
  if (args) {
    *args++ = '\0';
    while (*args == ' ') args++;
  } else {
    args = line + strlen(line);
  }

  if (strcmp(line, "help") == 0) {
    printFloatHarnessOK("commands=help,build_info,status,dump_config,mock_measurement,set_mock_sg,set_mock_temp,set_mock_battery,set_mock_angle,set_sequence,send_now,send_retry_buffer,dump_retry_buffer,clear_retry_buffer,simulate_ack,simulate_zero_ack,quick_zero,quick_zero_diag,zero_status,reset_zero_state,simulate_no_ack,no_sleep,pause_state,rx_debug,enter_calibration_mode,exit_calibration_mode,calib_status");
    return true;
  }
  if (strcmp(line, "build_info") == 0) {
    printFloatHarnessOK("build=float_ack_retry_harness_2026_05_23");
    return true;
  }
  if (strcmp(line, "status") == 0) {
    char msg[160];
    snprintf(msg, sizeof(msg), "status seq=%u rtcSeq=%u lastAck=%u retry=%d noSleep=%d paused=%d mock=%d calib=%d state=%d",
             sensorData.sequence_id, rtcSequenceCounter, lastAckedSeq, retryBufferCount(),
             harnessNoSleep ? 1 : 0, harnessPauseStateMachine ? 1 : 0,
             harnessMockEnabled ? 1 : 0, calibrationMode ? 1 : 0, (int)currentState);
    printFloatHarnessOK(msg);
    return true;
  }
  if (strcmp(line, "dump_config") == 0) {
    char msg[128];
    snprintf(msg, sizeof(msg), "config interval=%d ackWait=%d retrySize=%d maxRetries=%d",
             MEASUREMENT_INTERVAL, ACK_WAIT_MS, RETRY_BUFFER_SIZE, MAX_RETRIES);
    printFloatHarnessOK(msg);
    return true;
  }
  if (strcmp(line, "mock_measurement") == 0) {
    harnessMockEnabled = true;
    computeSensorData();
    printFloatHarnessOK("mock_measurement ready");
    return true;
  }
  if (strcmp(line, "set_mock_sg") == 0) {
    harnessMockSG = atof(args);
    harnessMockEnabled = true;
    printFloatHarnessOK("mock_sg set");
    return true;
  }
  if (strcmp(line, "set_mock_temp") == 0) {
    harnessMockTemp = atof(args);
    harnessMockEnabled = true;
    printFloatHarnessOK("mock_temp set");
    return true;
  }
  if (strcmp(line, "set_mock_battery") == 0) {
    harnessMockBattery = atof(args);
    harnessMockEnabled = true;
    printFloatHarnessOK("mock_battery set");
    return true;
  }
  if (strcmp(line, "set_mock_angle") == 0) {
    harnessMockAngle = atof(args);
    harnessMockEnabled = true;
    printFloatHarnessOK("mock_angle set");
    return true;
  }
  if (strcmp(line, "set_sequence") == 0) {
    rtcSequenceCounter = (uint16_t)atoi(args);
    sequence_counter = rtcSequenceCounter;
    printFloatHarnessOK("sequence set");
    return true;
  }
  if (strcmp(line, "send_now") == 0) {
    if (!harnessMockEnabled) {
      harnessMockEnabled = true;
    }
    computeSensorData();
    if (WiFi.getMode() == WIFI_OFF) initESPNow();
    bool acked = sendPayloadWithAck(&sensorData, true);
    if (acked) markAcked(sensorData.sequence_id);
    else storePayloadForRetry(sensorData);
    char msg[80];
    snprintf(msg, sizeof(msg), "send_now seq=%u ack=%d retry=%d", sensorData.sequence_id, acked ? 1 : 0, retryBufferCount());
    printFloatHarnessOK(msg);
    return true;
  }
  if (strcmp(line, "send_retry_buffer") == 0) {
    if (WiFi.getMode() == WIFI_OFF) initESPNow();
    int before = retryBufferCount();
    sendRetryPayloads(true);
    char msg[64];
    snprintf(msg, sizeof(msg), "retry_sent before=%d after=%d", before, retryBufferCount());
    printFloatHarnessOK(msg);
    return true;
  }
  if (strcmp(line, "dump_retry_buffer") == 0) {
    char msg[180];
    int used = snprintf(msg, sizeof(msg), "retry count=%d", retryBufferCount());
    for (int i = 0; i < RETRY_BUFFER_SIZE && used < (int)sizeof(msg) - 12; i++) {
      if (retryBuffer[i].valid) {
        used += snprintf(msg + used, sizeof(msg) - used, " %u", retryBuffer[i].payload.sequence_id);
      }
    }
    printFloatHarnessOK(msg);
    return true;
  }
  if (strcmp(line, "clear_retry_buffer") == 0) {
    memset(retryBuffer, 0, sizeof(retryBuffer));
    retryWriteIndex = 0;
    printFloatHarnessOK("retry_cleared");
    return true;
  }
  if (strcmp(line, "simulate_ack") == 0) {
    uint16_t seq = (uint16_t)atoi(args);
    markAcked(seq);
    printFloatHarnessOK("ack_simulated");
    return true;
  }
  if (strcmp(line, "simulate_zero_ack") == 0) {
    uint16_t seq = sensorData.sequence_id;
    uint8_t commandId = (uint8_t)(rtcLastAckCommandId + 1);
    if (args && args[0]) {
      seq = (uint16_t)atoi(args);
      char* second = strchr(args, ' ');
      if (second) {
        while (*second == ' ') second++;
        commandId = (uint8_t)atoi(second);
      }
    }

    ack_packet_t ack = {};
    ack.packet_type = ACK_PACKET_TYPE;
    ack.sequence_id = seq;
    ack.highest_seen = seq;
    ack.command = SG_ACK_COMMAND_ZERO_CALIBRATE;
    ack.command_id = commandId;

    zeroCalProcessedThisWake = false;
    onDataAck(baseStationMac, (const uint8_t*)&ack, sizeof(ack));
    bool processed = processQueuedZeroCalibrationCommand();
    char msg[160];
    snprintf(msg, sizeof(msg),
             "zero_ack seq=%u cmd=%u processed=%d flags=0x%02X offset=%d queued=%d",
             seq, commandId, processed ? 1 : 0, rtcPendingPayloadFlags,
             sensorOffsets.isValid ? 1 : 0, queuedZeroCalCommand ? 1 : 0);
    printFloatHarnessOK(msg);
    return true;
  }
  if (strcmp(line, "quick_zero") == 0) {
    float before = measureTilt();
    bool ok = runQuickZeroCalibration();
    float after = measureTilt();
    char msg[160];
    snprintf(msg, sizeof(msg), "quick_zero ok=%d before=%.2f after=%.2f offset=%d",
             ok ? 1 : 0, before, after, sensorOffsets.isValid ? 1 : 0);
    printFloatHarnessOK(msg);
    return true;
  }
  if (strcmp(line, "quick_zero_diag") == 0) {
    const int sampleCount = 5;
    long totalX = 0, totalY = 0, totalZ = 0;
    int minX = 32767, minY = 32767, minZ = 32767;
    int maxX = -32768, maxY = -32768, maxZ = -32768;
    for (int i = 0; i < sampleCount; i++) {
      int ax, ay, az;
      BMI160.readAccelerometer(ax, ay, az);
      totalX += ax; totalY += ay; totalZ += az;
      if (ax < minX) minX = ax; if (ay < minY) minY = ay; if (az < minZ) minZ = az;
      if (ax > maxX) maxX = ax; if (ay > maxY) maxY = ay; if (az > maxZ) maxZ = az;
      delay(40);
    }
    float avgX = totalX / (float)sampleCount;
    float avgY = totalY / (float)sampleCount;
    float avgZ = totalZ / (float)sampleCount;
    float mag = sqrtf(avgX * avgX + avgY * avgY + avgZ * avgZ) / 16384.0f;
    float newOffsetX = avgX;
    float newOffsetY = avgY + 16384.0f;
    float newOffsetZ = avgZ;
    float dx = sensorOffsets.isValid ? newOffsetX - sensorOffsets.offsetX : 0.0f;
    float dy = sensorOffsets.isValid ? newOffsetY - sensorOffsets.offsetY : 0.0f;
    float dz = sensorOffsets.isValid ? newOffsetZ - sensorOffsets.offsetZ : 0.0f;
    float delta = sensorOffsets.isValid ? sqrtf(dx * dx + dy * dy + dz * dz) : 0.0f;
    char msg[240];
    snprintf(msg, sizeof(msg),
             "quick_zero_diag mag=%.3f span=%d/%d/%d delta=%.0f raw=%.0f/%.0f/%.0f new=%.0f/%.0f/%.0f old=%.0f/%.0f/%.0f valid=%d",
             mag, maxX - minX, maxY - minY, maxZ - minZ, delta,
             avgX, avgY, avgZ, newOffsetX, newOffsetY, newOffsetZ,
             sensorOffsets.offsetX, sensorOffsets.offsetY, sensorOffsets.offsetZ,
             sensorOffsets.isValid ? 1 : 0);
    printFloatHarnessOK(msg);
    return true;
  }
  if (strcmp(line, "zero_status") == 0) {
    float angle = measureTilt();
    char msg[180];
    snprintf(msg, sizeof(msg),
             "zero_status angle=%.2f flags=0x%02X last_cmd=%u queued=%d queued_id=%u processed=%d offset=%d",
             angle, rtcPendingPayloadFlags, rtcLastAckCommandId,
             queuedZeroCalCommand ? 1 : 0, queuedZeroCalCommandId,
             zeroCalProcessedThisWake ? 1 : 0, sensorOffsets.isValid ? 1 : 0);
    printFloatHarnessOK(msg);
    return true;
  }
  if (strcmp(line, "reset_zero_state") == 0) {
    queuedZeroCalCommand = false;
    queuedZeroCalCommandId = 0;
    zeroCalProcessedThisWake = false;
    rtcPendingPayloadFlags &= ~(SG_PAYLOAD_FLAG_ZERO_CAL_OK | SG_PAYLOAD_FLAG_ZERO_CAL_FAIL);
    printFloatHarnessOK("zero_state_reset");
    return true;
  }
  if (strcmp(line, "simulate_no_ack") == 0) {
    storePayloadForRetry(sensorData);
    printFloatHarnessOK("no_ack_simulated");
    return true;
  }
  if (strcmp(line, "no_sleep") == 0) {
    harnessNoSleep = atoi(args) != 0;
    printFloatHarnessOK(harnessNoSleep ? "no_sleep on" : "no_sleep off");
    return true;
  }
  if (strcmp(line, "pause_state") == 0) {
    harnessPauseStateMachine = atoi(args) != 0;
    printFloatHarnessOK(harnessPauseStateMachine ? "state_paused" : "state_running");
    return true;
  }
  if (strcmp(line, "rx_debug") == 0) {
    harnessRxDebug = atoi(args) != 0;
    printFloatHarnessOK(harnessRxDebug ? "rx_debug on" : "rx_debug off");
    return true;
  }
  if (strcmp(line, "enter_calibration_mode") == 0) {
    calibrationMode = true;
    debug_mode = true;
    currentState = INIT;
    printFloatHarnessOK("calibration_mode on");
    return true;
  }
  if (strcmp(line, "exit_calibration_mode") == 0) {
    exitCalibrationMode();
    printFloatHarnessOK("calibration_mode off");
    return true;
  }
  if (strcmp(line, "calib_status") == 0) {
    char msg[96];
    snprintf(msg, sizeof(msg), "calib mode=%d points=%d valid=%d offset=%d",
             calibrationMode ? 1 : 0, numCalibPoints, isCalibrationValid() ? 1 : 0,
             isSensorOffsetValid() ? 1 : 0);
    printFloatHarnessOK(msg);
    return true;
  }

  printFloatHarnessERR("unknown_command");
  return false;
}
#endif

// Helper functions for calibration mode management
void toggleCalibrationMode() {
  calibrationMode = !calibrationMode;
  debug_mode = calibrationMode;  // Debug mode follows calibration mode
  
  if (calibrationMode) {
    // Entering calibration mode
    if (debug_mode) {
      if (!Serial) Serial.begin(115200);
    }
    if (debug_mode) Serial.println("Button pressed - Deep sleep disabled");
        
    lastCalibrationActivity = millis(); // Reset timeout counter
    currentState = INIT; // Restart state machine
    
    // Trigger calibration sequence on base station by sending initial command
    sendCalibrationTrigger();
  } else {
    // Exiting calibration mode
    if (debug_mode) Serial.println("Deep sleep enabled - Power saving mode");
    
    digitalWrite(LED_BUILTIN, LOW); // Turn off LED
    currentState = INIT; // Restart state machine
  }
}

void exitCalibrationMode() {
  calibrationMode = false;
  debug_mode = false;
  
  if (debug_mode) Serial.println("Auto-exit - Deep sleep enabled");
  
  digitalWrite(LED_BUILTIN, LOW); // Turn off LED
  currentState = INIT; // Restart state machine
}

void sendCalibrationTrigger() {
  ensureDebugSerial();
  Serial.println("=== SENDING CALIBRATION TRIGGER ===");
  Serial.printf("Target MAC: %02X:%02X:%02X:%02X:%02X\n", 
                baseStationMac[0], baseStationMac[1], baseStationMac[2], 
                baseStationMac[3], baseStationMac[4], baseStationMac[5]);
  
  // Send a special calibration trigger to base station
  calib_command_t triggerCmd;
  triggerCmd.command = 6; // Use command 6 as CALIBRATION_TRIGGER
  triggerCmd.target_sg = 0.0;
  triggerCmd.request_id = 255; // Special ID for trigger
  
  if (debug_mode) {
    Serial.printf("Sending command: %d, target_sg: %.3f, request_id: %d\n", 
                  triggerCmd.command, triggerCmd.target_sg, triggerCmd.request_id);
  }
  
  esp_err_t result = esp_now_send(baseStationMac, (uint8_t *)&triggerCmd, sizeof(triggerCmd));
  
  if (result == ESP_OK) {
    Serial.println("Calibration trigger sent - waiting for base station response...");
  } else {
    Serial.printf("Error sending calibration trigger: %d\n", result);
    Serial.println("Check base station is powered and in range");
  }
}

void updateCalibrationActivity() {
  lastCalibrationActivity = millis();
  if (debug_mode) Serial.println("Calibration activity timestamp updated");
}

// Exponential Moving Average filter
float ema(float prev, float x, float alpha) {
  return alpha * x + (1 - alpha) * prev;
}
