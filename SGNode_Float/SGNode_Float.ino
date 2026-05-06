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
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <BMI160Gen.h>
#include "polynomial_calibration.h"
#include "../SGNode_Shared/sg_protocol.h"

// I2C pins for BMI160
#define I2C_SDA 26
#define I2C_SCL 27

// System configuration
#define MEASUREMENT_INTERVAL 180  // seconds (configurable 60-600)
#define SAMPLE_COUNT 5            // samples per measurement (reduced for power efficiency)
#define MAX_RETRIES 1             // transmission retry attempts
#define MAX_INIT_RETRIES 3        // sensor initialization retry attempts
#define BATTERY_PIN 15            // Battery voltage monitoring (GPIO15 - ADC1_CH3)
#define CALIBRATION_SWITCH_PIN 12  // Physical switch for calibration mode (pull-down)

// LED indicators (WeMos D32)
#define LED_BUILTIN 16           // Built-in LED - blinks during calibration mode

// ESP-NOW configuration
#define ESPNOW_CHANNEL 1

// Base station MAC address (update with actual address)
uint8_t baseStationMac[] = {0xA4, 0xF0, 0x0F, 0x68, 0x22, 0x00};

payload_t sensorData;

// BMP180 instance
Adafruit_BMP085 bmp180;

// BMI160 instance provided by EmotiBit_BMI160 library
// No need to declare - library provides global BMI160 instance

// Calibration system variables (actual definitions)
CalibrationCoefficients calibCoeffs;
CalibrationPoint calibPoints[MAX_CALIB_POINTS];
int numCalibPoints = 0;

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

// Rate limiting for calibration responses
unsigned long lastCalibrationResponse = 0;
const unsigned long CALIBRATION_RESPONSE_INTERVAL = 2000; // 2 seconds between responses
bool calibrationResponsePending = false;

// Function prototypes
void initIMU();
void initBMP180();
void initESPNow();
void configureUnusedGPIOs();
float measureTilt();
float measureTemperature();
float calculateDensity(float angle, float temperature);
float getBatteryVoltage();
void computeSensorData();
void transmitData();
void enterDeepSleep();
void onCalibrationCommand(const uint8_t *mac, const uint8_t *incomingData, int len);
void sendCalibrationResponse(float angle, float target_sg, uint8_t request_id, const char* message);
uint16_t crc16(const uint8_t* data, size_t length);
float medianFilter(float arr[], int n);
float ema(float prev, float x, float alpha);

void setup() {
  // Configure unused GPIOs FIRST to avoid conflicts
  configureUnusedGPIOs();
  
  // Configure pins (after unused GPIOs are set)
  pinMode(BATTERY_PIN, INPUT);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);  // Pin 15 is ADC1_CH3 - can use attenuation
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
    Serial.println("Woke up from deepsleep - calibration switch detected");
  }
  
  // Read battery voltage BEFORE WiFi is enabled (no interference)
  cachedBatteryVoltage = getBatteryVoltage();
  Serial.printf("Initial battery voltage: %.2fV\n", cachedBatteryVoltage);
  
  // Initialize push button state
  lastButtonState = digitalRead(CALIBRATION_SWITCH_PIN);
  
  // Check calibration switch state at boot (before deepsleep)
  bool switchPressed = (digitalRead(CALIBRATION_SWITCH_PIN) == LOW);
  
  // Activate calibration mode if switch is pressed OR woke up from calibration interrupt
  if (switchPressed || calibrationWakeup) {
    calibrationMode = true;
    debug_mode = true;
    Serial.println("=== CALIBRATION MODE ACTIVATED AT BOOT ===");
    if (switchPressed) {
      Serial.println("Switch pressed at boot");
    } else {
      Serial.println("Woke up from calibration interrupt");
    }
    lastCalibrationActivity = millis();
    
    // IMPORTANT: Send calibration trigger after ESP-NOW is initialized
    // This will be called in the state machine after initESPNow()
  } else {
    calibrationMode = false;
    debug_mode = true; // Enable debug mode to see button state readings
  }
  
  // Always start Serial for basic monitoring
  Serial.begin(115200);
  delay(100);  // Wait for Serial
  
  if (debug_mode) {
    Serial.println("=== Fermentation Float Unit Starting ===");
    Serial.printf("Base Station MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  baseStationMac[0], baseStationMac[1], baseStationMac[2],
                  baseStationMac[3], baseStationMac[4], baseStationMac[5]);
  } else {
    Serial.println("=== Fermentation Float Unit ===");
  }
  
  // Disable Bluetooth
  btStop();
  delay(50);
  
  // Initialize calibration system
  initCalibration();
  
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
    if (debug_mode) Serial.println("=== CALIBRATION MODE ACTIVATED ===");
    if (debug_mode) Serial.println("Button activated - Deep sleep disabled");
    if (debug_mode) Serial.println("WARNING: High battery drain expected!");
    lastCalibrationActivity = millis(); // Initialize timeout counter
    if (isCalibrationValid()) {
      if (debug_mode) Serial.println("Valid calibration found from EEPROM");
      if (debug_mode) printCalibrationCoefficients();
    } else {
      if (debug_mode) Serial.println("No valid calibration - ready for new calibration");
    }
  }
  
  if (debug_mode) Serial.println("Hardware initialized");
}

void loop() {
  
  // Handle push button detection with debouncing
  bool currentButtonState = digitalRead(CALIBRATION_SWITCH_PIN);
  
  // Debug button state (remove after testing) - only in non-calibration mode
  static unsigned long lastDebugPrint = 0;
  if (!calibrationMode && millis() - lastDebugPrint > 1000) {
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
        Serial.println("Button press detected!");
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
      delay(3000);  // 3s stabilization delay after wake
      initIMU();
      initBMP180();
      initESPNow();
      
      // Send calibration trigger if in calibration mode and ESP-NOW is ready
      if (calibrationMode) {
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
      // Only transmit normal data if NOT in calibration mode
      if (!calibrationMode) {
        if (debug_mode) Serial.println("Transmitting normal sensor data");
        transmitData();
        currentState = SLEEP;
      } else {
        // In calibration mode, check for pending responses and continue measuring
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

void initBMP180() {
  // BMP180 - simplified for testing
  if (debug_mode) Serial.println("BMP180 init...");
  
  if (bmp180.begin()) {
    if (debug_mode) Serial.println("BMP180 OK");
  } else {
    if (debug_mode) Serial.println("BMP180 failed");
    sensorData.flags |= 0x02;
  }
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
      if (addResult == ESP_ERR_ESPNOW_NOT_INIT) Serial.println("  ESP-NOW not initialized");
      if (addResult == ESP_ERR_ESPNOW_IF) Serial.println("  Invalid WiFi interface");
      if (addResult == ESP_ERR_INVALID_ARG) Serial.println("  Invalid argument");
      if (addResult == ESP_ERR_ESPNOW_NO_MEM) Serial.println("  Out of memory");
      if (addResult == ESP_ERR_ESPNOW_FULL) Serial.println("  Peer list full");
      if (addResult == ESP_ERR_ESPNOW_EXIST) Serial.println("  Peer already exists");
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
    if (debug_mode) Serial.println("ESP-NOW initialized (transmit only)");
  }
}

void configureUnusedGPIOs() {
  // Configure unused GPIOs as OUTPUT LOW to minimize power consumption
  // Used GPIOs: 12(switch), 16(LED), 26(I2C_SDA), 27(I2C_SCL), 35(battery)
  // Skip: strapping pins (0, 2, 15), FLASH pins (6-11), input-only pins (34-39), non-existent pins
  
  const uint8_t unusedGpios[] = {
    3,              // Safe unused RTC (pin 4 used for battery)
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

float measureTilt() {
  // Read from BMI160 using library
  int ax, ay, az;
  BMI160.readAccelerometer(ax, ay, az);
  
  if (ax == 0 && ay == 0 && az == 0) {
    if (debug_mode) Serial.println("BMI160 reading zeros - possible error");
    return 45.0; // Fallback
  }
  
  // Convert to tilt angle (deviation from vertical) - clean approach
  float ax_f = ax / 16384.0;
  float ay_f = ay / 16384.0;
  float az_f = az / 16384.0;
  float magnitude = sqrt(ax_f*ax_f + ay_f*ay_f + az_f*az_f);
  float angle_rad = acos(ay_f / magnitude);  // Angle from Y-axis
  float angle_deg = angle_rad * 180.0 / PI;
  angle_deg = 180.0 - angle_deg;  // Invert for correct tilt measurement
  
  // Only show tilt data if NOT in calibration mode
  if (!calibrationMode) {
    Serial.printf("Tilt: %.2f°\n", angle_deg);
    if (debug_mode) {
      Serial.printf("  Raw: ax=%d, ay=%d, az=%d\n", ax, ay, az);
    }
  }
  
  return angle_deg;
}

float measureTemperature() {
  // Read from BMP180
  float temperature = bmp180.readTemperature();
  
  if (isnan(temperature)) {
    if (debug_mode) Serial.println("Failed to read temperature from BMP180");
    sensorData.flags |= 0x02;
    return ema_temp; // Return last valid value
  }
  
  if (temperature < 0.0 || temperature > 50.0) {
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
  // Read ADC value - pin 15 (ADC1_CH3) with 11dB attenuation
  int adc_value = analogRead(BATTERY_PIN);
  
  // Always show raw ADC value for battery debugging
  Serial.printf("Raw ADC pin 15: %d", adc_value);
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
  sensorData.temperature = measureTemperature();
  sensorData.density = calculateDensity(sensorData.angle, sensorData.temperature);
  sensorData.uptime_s = (millis() / 1000) - boot_time;
  sensorData.sequence_id = sequence_counter++;
  sensorData.version = SG_PROTOCOL_VERSION;
  sensorData.flags = 0;  // Clear flags
  
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
    sensorData.flags |= 0x01;  // Set delayed flag
  }
  
  delay(50);
  
  // Turn off WiFi completely (only in normal mode)
  if (!calibrationMode) {
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    delay(100);  // Wait for WiFi to fully shut down
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
      
    case 4: // APPLY_CALIBRATION (calculate 3rd degree polynomial)
      if (debug_mode) Serial.println("Calculating 3rd degree polynomial coefficients using least squares");
      if (calculatePolynomialCoefficients()) {
        saveCalibrationCoefficients();
        testCalibrationAccuracy();
        
        // Reset rate limiting to ensure final response gets through
        lastCalibrationResponse = 0;
        calibrationResponsePending = false;
        
        sendCalibrationResponse(0, 0, cmd.request_id, "3rd degree polynomial calculated and saved");
        
        // Auto-exit calibration mode after successful apply
        if (debug_mode) Serial.println("Calibration completed - auto-exiting calibration mode");
        exitCalibrationMode();
      } else {
        // Reset rate limiting for error response too
        lastCalibrationResponse = 0;
        calibrationResponsePending = false;
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

// Helper functions for calibration mode management
void toggleCalibrationMode() {
  calibrationMode = !calibrationMode;
  debug_mode = calibrationMode;  // Debug mode follows calibration mode
  
  if (calibrationMode) {
    // Entering calibration mode
    if (debug_mode) {
      if (!Serial) Serial.begin(115200);
      Serial.println("=== CALIBRATION MODE ACTIVATED ===");
    }
    if (debug_mode) Serial.println("Button pressed - Deep sleep disabled");
    if (debug_mode) Serial.println("WARNING: High battery drain expected!");
    
    lastCalibrationActivity = millis(); // Reset timeout counter
    currentState = INIT; // Restart state machine
    
    // Trigger calibration sequence on base station by sending initial command
    sendCalibrationTrigger();
  } else {
    // Exiting calibration mode
    if (debug_mode) Serial.println("=== CALIBRATION MODE DEACTIVATED ===");
    if (debug_mode) Serial.println("Deep sleep enabled - Power saving mode");
    
    digitalWrite(LED_BUILTIN, LOW); // Turn off LED
    currentState = INIT; // Restart state machine
  }
}

void exitCalibrationMode() {
  calibrationMode = false;
  debug_mode = false;
  
  if (debug_mode) Serial.println("=== CALIBRATION MODE TIMEOUT ===");
  if (debug_mode) Serial.println("Auto-exit - Deep sleep enabled");
  
  digitalWrite(LED_BUILTIN, LOW); // Turn off LED
  currentState = INIT; // Restart state machine
}

void sendCalibrationTrigger() {
  Serial.println("=== SENDING CALIBRATION TRIGGER ===");
  Serial.printf("Target MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                baseStationMac[0], baseStationMac[1], baseStationMac[2], 
                baseStationMac[3], baseStationMac[4], baseStationMac[5]);
  
  // Send a special calibration trigger to base station
  calib_command_t triggerCmd;
  triggerCmd.command = 5; // Use command 5 as CALIBRATION_TRIGGER
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