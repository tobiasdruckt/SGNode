/*
 * Fermentation Monitor - Float Unit (Sensor)
 * Measures tilt via BMI160 IMU and transmits via ESP-NOW
 * Battery-powered with deep sleep for power efficiency
 * 
 * REQUIRED LIBRARIES (install via Arduino Library Manager):
 * - EmotiBit BMI160 (by Connected Future Labs) - Install from:
 *   https://github.com/EmotiBit/EmotiBit_BMI160
 *   Download as ZIP and install via Sketch -> Include Library -> Add .ZIP Library
 * - Adafruit BMP085 Library (by Adafruit)
 * - Wire (built-in)
 * - EEPROM (built-in)
 */

#include <WiFi.h>
#include <esp_now.h>
#include <BMI160Gen.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <esp_task_wdt.h>
#include "polynomial_calibration.h"

// I2C pins for BMI160
#define I2C_SDA 21
#define I2C_SCL 22

// System configuration
#define MEASUREMENT_INTERVAL 180  // seconds (configurable 60-600)
#define SAMPLE_COUNT 5            // samples per measurement (reduced for power efficiency)
#define MAX_RETRIES 1             // transmission retry attempts
#define MAX_INIT_RETRIES 3        // sensor initialization retry attempts
#define BATTERY_PIN 35           // Battery voltage monitoring (WeMos D32 built-in)
#define CALIBRATION_SWITCH_PIN 12  // Physical switch for calibration mode (pull-down)

// LED indicators (WeMos D32)
#define LED_BUILTIN 5            // Built-in LED
#define LED_EXTRA 16             // Extra programmable LED

// ESP-NOW configuration
#define ESPNOW_CHANNEL 1

// Base station MAC address (update with actual address)
uint8_t baseStationMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Data structure for transmission
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

payload_t sensorData;

// BMI160 instance provided by EmotiBit_BMI160 library
// No need to declare - library provides global BMI160 instance
Adafruit_BMP085 bmp180;

// Global variables for filtering and tracking
uint16_t sequence_counter = 0;
uint32_t boot_time = 0;
float ema_temp = 20.0;  // Initial temperature for EMA
const float EMA_ALPHA = 0.2;  // EMA alpha between 0.1-0.3
bool debug_mode = false;  // Debug mode enabled by calibration switch

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

// Function prototypes
void initIMU();
void initBMP180();
void initESPNow();
void configureUnusedGPIOs();
float measureTilt();
float measureTemperature();
float calculateDensity(float angle, float temperature);
float getBatteryVoltage();
void transmitData();
void enterDeepSleep();
void onCalibrationCommand(const uint8_t *mac, const uint8_t *incomingData, int len);
void sendCalibrationResponse(float angle, float target_sg, uint8_t request_id, const char* message);
uint16_t crc16(const uint8_t* data, size_t length);
float medianFilter(float arr[], int n);
float ema(float prev, float x, float alpha);

void setup() {
  // Configure pins first
  pinMode(BATTERY_PIN, INPUT);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
  pinMode(CALIBRATION_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_EXTRA, OUTPUT);
  
  // Initialize LEDs
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(LED_EXTRA, LOW);
  
  // Check calibration switch first to determine debug mode
  calibrationMode = (digitalRead(CALIBRATION_SWITCH_PIN) == LOW);
  debug_mode = calibrationMode;  // Debug mode enabled by calibration switch
  
  if (debug_mode) {
    Serial.begin(115200);
    Serial.println("=== Fermentation Float Unit Starting ===");
    Serial.println("DEBUG MODE: Serial logging enabled");
  }
  
  // Disable Bluetooth for power saving
  btStop();
  
  // Initialize hardware watchdog (30s timeout) - new ESP-IDF signature
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 30000,
    .idle_core_mask = 0,  // Watch all cores
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  
  // Initialize calibration system first
  initCalibration();
  
  // Configure I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000); // 100 kHz I2C
  
  // Check I2C bus is working
  Wire.beginTransmission(0x68); // BMI160 default address
  if (Wire.endTransmission() != 0) {
    if (debug_mode) Serial.println("I2C bus check failed - no device detected");
  } else {
    if (debug_mode) Serial.println("I2C bus initialized successfully");
  }
  
  // Configure pins
  pinMode(BATTERY_PIN, INPUT);
  pinMode(CALIBRATION_SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_EXTRA, OUTPUT);
  
  // Initialize LEDs
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(LED_EXTRA, LOW);
  
  // Configure unused GPIOs as OUTPUT LOW to minimize power consumption
  configureUnusedGPIOs();
  
  // Record boot time
  boot_time = millis() / 1000;
  
  if (calibrationMode) {
    if (debug_mode) Serial.println("=== CALIBRATION MODE ACTIVATED ===");
    if (debug_mode) Serial.println("Physical switch detected - Deep sleep disabled");
    if (debug_mode) Serial.println("WARNING: High battery drain expected!");
    digitalWrite(LED_EXTRA, HIGH);
    if (isCalibrationValid()) {
      if (debug_mode) Serial.println("Valid calibration found from EEPROM");
      if (debug_mode) printCalibrationCoefficients();
    } else {
      if (debug_mode) Serial.println("No valid calibration - ready for new calibration");
    }
  } else {
    digitalWrite(LED_EXTRA, LOW);
  }
  
  if (debug_mode) Serial.println("Hardware initialized");
}

void loop() {
  // Reset watchdog at start of each cycle
  esp_task_wdt_reset();
  
  // Check calibration switch state
  bool currentSwitchState = (digitalRead(CALIBRATION_SWITCH_PIN) == LOW);
  
  if (currentSwitchState != calibrationMode) {
    calibrationMode = currentSwitchState;
    debug_mode = calibrationMode;  // Debug mode follows calibration mode
    
    if (calibrationMode) {
      if (debug_mode) {
        if (!Serial) Serial.begin(115200);
        Serial.println("=== CALIBRATION MODE ACTIVATED ===");
      }
      if (debug_mode) Serial.println("Deep sleep disabled - High battery drain!");
      digitalWrite(LED_EXTRA, HIGH);
      currentState = INIT;
    } else {
      if (debug_mode) Serial.println("=== NORMAL MODE RESUMED ===");
      if (debug_mode) Serial.println("Deep sleep enabled - Power saving mode");
      digitalWrite(LED_EXTRA, LOW);
      currentState = INIT;
    }
  }
  
  // Normal operation
  switch (currentState) {
    case INIT:
      if (debug_mode) Serial.println("State: INIT");
      delay(3000);  // 3s stabilization delay after wake
      initIMU();
      initBMP180();
      initESPNow();
      currentState = MEASURE;
      break;
      
    case MEASURE:
      if (debug_mode) Serial.println("State: MEASURE");
      sensorData.angle = measureTilt();
      currentState = COMPUTE;
      break;
      
    case COMPUTE:
      if (debug_mode) Serial.println("State: COMPUTE");
      sensorData.temperature = measureTemperature();
      sensorData.density = calculateDensity(sensorData.angle, sensorData.temperature);
      sensorData.battery_voltage = getBatteryVoltage();
      sensorData.uptime_s = (millis() / 1000) - boot_time;
      sensorData.sequence_id = sequence_counter++;
      sensorData.version = 2;  // Protocol version 2
      sensorData.flags = 0;  // Clear flags
      currentState = SEND;
      break;
      
    case SEND:
      if (debug_mode) Serial.println("State: SEND");
      transmitData();
      
      if (!calibrationMode) {
        currentState = SLEEP;
      } else {
        currentState = MEASURE;
        delay(1000);
      }
      break;
      
    case SLEEP:
      if (debug_mode) Serial.println("State: SLEEP");
      enterDeepSleep();
      break;
  }
  
  delay(10);
}

void initIMU() {
  for (int i = 0; i < MAX_INIT_RETRIES; i++) {
    if (BMI160.begin(BMI160GenClass::I2C_MODE)) {
      if (debug_mode) Serial.println("BMI160 initialized successfully");
      BMI160.setAccelerometerRange(2);  // 2G range
      BMI160.setAccelerometerRate(25);  // 25Hz for power saving
      if (debug_mode) Serial.println("IMU ready");
      return;
    }
    delay(1000);
  }
  
  if (debug_mode) Serial.println("BMI160 initialization failed after retries");
  sensorData.flags |= 0x02;  // Set sensor error flag
}

void initBMP180() {
  for (int i = 0; i < MAX_INIT_RETRIES; i++) {
    if (bmp180.begin()) {
      // BMP085 library doesn't have setSampling, uses default settings
      if (debug_mode) Serial.println("BMP180 ready");
      return;
    }
    delay(1000);
  }
  
  if (debug_mode) Serial.println("BMP180 initialization failed after retries");
  sensorData.flags |= 0x02;  // Set sensor error flag
}

void initESPNow() {
  WiFi.mode(WIFI_STA);
  // esp_wifi_set_channel is deprecated in newer ESP-IDF versions
  // ESP-NOW will automatically use the current WiFi channel
  
  if (esp_now_init() != ESP_OK) {
    if (debug_mode) Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, baseStationMac, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    if (debug_mode) Serial.println("Failed to add peer");
    return;
  }
  
  if (calibrationMode) {
    // Register receive callback (new ESP-IDF signature)
    esp_now_register_recv_cb([](const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
      onCalibrationCommand(recv_info->src_addr, data, len);
    });
    if (debug_mode) Serial.println("ESP-NOW initialized with calibration support");
  } else {
    if (debug_mode) Serial.println("ESP-NOW initialized (transmit only)");
  }
}

void configureUnusedGPIOs() {
  // Configure unused GPIOs as OUTPUT LOW to minimize power consumption
  // Used GPIOs: 5(LED), 12(switch), 16(LED), 21(I2C_SDA), 22(I2C_SCL), 35(battery)
  // Skip strapping pins (0, 2, 12, 15), FLASH pins (6-11), and input-only pins (34-39)
  
  const uint8_t unusedGpios[] = {
    1, 3, 4,           // Safe unused
    13, 14,             // Safe unused (14 is JTAG, can be used)
    17, 18, 19, 20,     // Safe unused
    23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33  // Safe unused
  };
  
  for (int i = 0; i < sizeof(unusedGpios) / sizeof(unusedGpios[0]); i++) {
    pinMode(unusedGpios[i], OUTPUT);
    digitalWrite(unusedGpios[i], LOW);
  }
  
  if (debug_mode) Serial.println("Unused GPIOs configured as OUTPUT LOW for power saving");
}

float measureTilt() {
  float angles[SAMPLE_COUNT];
  int valid_samples = 0;
  
  if (debug_mode) Serial.printf("Taking %d samples...", SAMPLE_COUNT);
  
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    int ax_raw, ay_raw, az_raw;
    
    BMI160.readAccelerometer(ax_raw, ay_raw, az_raw);
    
    // Check if values are valid (non-zero)
    if (ax_raw != 0 || ay_raw != 0 || az_raw != 0) {
      float ax = ax_raw / 16384.0;
      float ay = ay_raw / 16384.0;
      float az = az_raw / 16384.0;
      
      // Improved 3D tilt formula using all axes
      float angle_rad = atan2(ax, sqrt(ay*ay + az*az));
      angles[valid_samples++] = angle_rad * 180.0 / PI;
      
      delay(200);  // 200ms between samples
    } else {
      if (debug_mode) Serial.println("Failed to read accelerometer");
    }
  }
  
  if (valid_samples == 0) {
    if (debug_mode) Serial.println("No valid samples");
    sensorData.flags |= 0x02;  // Set sensor error flag
    return 0.0;
  }
  
  float angle = medianFilter(angles, valid_samples);
  
  if (debug_mode) Serial.printf("Tilt: %.2f° (median of %d samples)\n", angle, valid_samples);
  
  return angle;
}

float measureTemperature() {
  float temperature = bmp180.readTemperature();
  
  if (isnan(temperature)) {
    if (debug_mode) Serial.println("Failed to read temperature from BMP180");
    sensorData.flags |= 0x02;  // Set sensor error flag
    return ema_temp;  // Return last valid value
  }
  
  if (temperature < 0.0 || temperature > 50.0) {
    if (debug_mode) Serial.printf("Temperature out of range: %.2f°C\n", temperature);
    sensorData.flags |= 0x02;  // Set sensor error flag
    return ema_temp;  // Return last valid value
  }
  
  // Apply EMA filter
  ema_temp = ema(ema_temp, temperature, EMA_ALPHA);
  
  if (debug_mode) Serial.printf("Temperature: %.2f°C (EMA)\n", ema_temp);
  
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
  
  if (debug_mode) Serial.printf("Density: Raw SG=%.4f, Temp=%.2f°C, Final SG=%.4f\n",
                sg, temperature, correctedSG);

  return correctedSG;
}

float getBatteryVoltage() {
  int adc_value = analogRead(BATTERY_PIN);
  float voltage = adc_value * 3.3 / 4095.0 * 2.0;  // 2:1 voltage divider
  
  const float voltage_calibration = 1.0;
  voltage *= voltage_calibration;
  
  // Check voltage thresholds and set flags
  if (voltage < 3.3) {
    sensorData.flags |= 0x04;  // Set low battery flag
  }
  
  if (debug_mode) Serial.printf("Battery: %.2fV (ADC: %d)\n", voltage, adc_value);
  
  return voltage;
}

void transmitData() {
  // Calculate CRC before transmission (exclude version from CRC for backward compatibility)
  size_t payload_size = sizeof(sensorData) - sizeof(sensorData.crc) - sizeof(sensorData.version);
  sensorData.crc = crc16((const uint8_t*)&sensorData + sizeof(sensorData.version), payload_size);
  
  if (debug_mode) Serial.println("Transmitting data...");
  
  // Send data via ESP-NOW with retry
  esp_err_t result;
  for (int i = 0; i <= MAX_RETRIES; i++) {
    result = esp_now_send(baseStationMac, (uint8_t *)&sensorData, sizeof(sensorData));
    
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
    if (debug_mode) Serial.printf("Error sending data after retries: %d\n", result);
    sensorData.flags |= 0x01;  // Set delayed flag
  }
  
  delay(50);
  
  // Turn off WiFi completely (only in normal mode)
  if (!calibrationMode) {
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
  }
}

void enterDeepSleep() {
  if (debug_mode) Serial.println("Entering deep sleep...");
  
  // Ensure WiFi/ESP-NOW is fully deinitialized before sleep
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
  // esp_wifi_stop is deprecated in newer ESP-IDF versions
  
  // Adjust sleep interval based on battery voltage
  uint64_t sleep_duration = MEASUREMENT_INTERVAL * 1000000ULL;
  if (sensorData.flags & 0x04) {  // Low battery flag
    sleep_duration *= 2;  // Double interval at low voltage
    if (debug_mode) Serial.println("Low battery: doubling sleep interval");
  }
  
  // Flush Serial to ensure all debug output is sent before sleep
  if (debug_mode) Serial.flush();
  
  esp_sleep_enable_timer_wakeup(sleep_duration);
  esp_deep_sleep_start();
}

// Remote calibration command handler (only active in calibration mode)
void onCalibrationCommand(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (!calibrationMode) {
    return;
  }
  
  if (len != sizeof(calib_command_t)) {
    if (debug_mode) Serial.println("Invalid calibration command size");
    return;
  }
  
  calib_command_t cmd;
  memcpy(&cmd, incomingData, len);
  
  if (debug_mode) Serial.printf("Received calibration command: %d, target_sg=%.3f, request_id=%d\n", 
                cmd.command, cmd.target_sg, cmd.request_id);
  
  switch (cmd.command) {
    case 0: // CALIBRATE_POINT1 (Water SG=1.000)
      if (debug_mode) Serial.println("Point 1 calibration measurement requested (SG=1.000)");
      {
        float angle = measureTilt();
        float temp = measureTemperature();
        addCalibrationPoint(angle, cmd.target_sg, temp);
        sendCalibrationResponse(angle, cmd.target_sg, cmd.request_id, "Point 1 added");
      }
      break;
      
    case 1: // CALIBRATE_POINT2 (Light Sugar SG=1.040)
      if (debug_mode) Serial.println("Point 2 calibration measurement requested (SG=1.040)");
      {
        float angle = measureTilt();
        float temp = measureTemperature();
        addCalibrationPoint(angle, cmd.target_sg, temp);
        sendCalibrationResponse(angle, cmd.target_sg, cmd.request_id, "Point 2 added");
      }
      break;
      
    case 2: // CALIBRATE_POINT3 (Medium Sugar SG=1.080)
      if (debug_mode) Serial.println("Point 3 calibration measurement requested (SG=1.080)");
      {
        float angle = measureTilt();
        float temp = measureTemperature();
        addCalibrationPoint(angle, cmd.target_sg, temp);
        sendCalibrationResponse(angle, cmd.target_sg, cmd.request_id, "Point 3 added");
      }
      break;
      
    case 3: // CALIBRATE_POINT4 (Heavy Sugar SG=1.120)
      if (debug_mode) Serial.println("Point 4 calibration measurement requested (SG=1.120)");
      {
        float angle = measureTilt();
        float temp = measureTemperature();
        addCalibrationPoint(angle, cmd.target_sg, temp);
        sendCalibrationResponse(angle, cmd.target_sg, cmd.request_id, "Point 4 added");
      }
      break;
      
    case 4: // APPLY_CALIBRATION (calculate 3rd degree polynomial)
      if (debug_mode) Serial.println("Calculating 3rd degree polynomial coefficients using least squares");
      if (calculatePolynomialCoefficients()) {
        saveCalibrationCoefficients();
        testCalibrationAccuracy();
        sendCalibrationResponse(0, 0, cmd.request_id, "3rd degree polynomial calculated and saved");
      } else {
        sendCalibrationResponse(0, 0, cmd.request_id, "Error: Need at least 4 calibration points");
      }
      break;
      
    default:
      if (debug_mode) Serial.println("Unknown calibration command");
      sendCalibrationResponse(0, 0, cmd.request_id, "Unknown command");
      break;
  }
}

void sendCalibrationResponse(float angle, float target_sg, uint8_t request_id, const char* message) {
  calib_response_t response;
  response.response_type = 0; // CALIBRATION_DATA
  response.angle = angle;
  response.sg = target_sg;
  response.request_id = request_id;
  strncpy(response.message, message, sizeof(response.message) - 1);
  response.message[sizeof(response.message) - 1] = '\0';
  
  esp_err_t result = esp_now_send(baseStationMac, (uint8_t *)&response, sizeof(response));
  
  if (result == ESP_OK) {
    if (debug_mode) Serial.printf("Calibration response sent: angle=%.2f°, sg=%.3f, msg=%s\n", 
                  angle, target_sg, message);
  } else {
    if (debug_mode) Serial.printf("Error sending calibration response: %d\n", result);
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

// Median filter for noise reduction
float medianFilter(float arr[], int n) {
  // Simple bubble sort for small arrays
  for (int i = 0; i < n-1; i++) {
    for (int j = 0; j < n-i-1; j++) {
      if (arr[j] > arr[j+1]) {
        float temp = arr[j];
        arr[j] = arr[j+1];
        arr[j+1] = temp;
      }
    }
  }
  return arr[n/2];  // Return median
}

// Exponential Moving Average filter
float ema(float prev, float x, float alpha) {
  return alpha * x + (1 - alpha) * prev;
}