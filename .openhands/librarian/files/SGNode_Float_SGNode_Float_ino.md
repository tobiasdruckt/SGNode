# SGNode_Float/SGNode_Float.ino

**Type:** Arduino Sketch
**Hash:** `766b927d8caee6c782ad5918f859abfb233d3e6ea66a65b48d30be009dc58318`

## Overview / Role

Core functionality module

## Verified API & Symbols

### Structs & Classes

- `struct StoredPayload`
  - `bool valid`
  - `payload_t payload`

- `struct PendingStableCalibrationPoint`
  - `bool active`
  - `float targetSG`
  - `uint8_t requestId`
  - `uint8_t pointNumber`
  - `unsigned long startedMs`
  - `unsigned long lastSampleMs`
  - `int sampleCount`
  - `float firstAngle`
  - `float firstTemp`
  - `float firstSG`
  - `float lastAngle`
  - `float lastTemp`
  - `float lastSG`
  - `float sumAngle`
  - `float sumTemp`

- `struct PendingStableOffsetCalibration`
  - `bool active`
  - `uint8_t requestId`
  - `unsigned long startedMs`
  - `unsigned long lastSampleMs`
  - `int sampleCount`
  - `float firstX`
  - `float firstY`
  - `float firstZ`
  - `float lastX`
  - `float lastY`
  - `float lastZ`
  - `float firstTemp`
  - `float lastTemp`
  - `float previousTempDrift`
  - `unsigned long previousTempDriftMs`
  - `float peakTempDrift`
  - `float sumX`
  - `float sumY`
  - `float sumZ`
  - `float sumTemp`
  - `int minX`
  - `int minY`
  - `int minZ`
  - `int maxX`
  - `int maxY`
  - `int maxZ`

### Functions

- `void setup (void)`

- `void loop (void)`

- `void detectOtaLongPressAtBoot (bool switchPressed)`

- `void floatOtaDiagHtml (void)`

- `void floatOtaPageHtml (void)`

- `void sendFloatOtaActiveStatus (void)`

- `void startFloatOtaMode (void)`

- `void on ("/", HTTP_GET, []()`

- `void on ("/diag", HTTP_GET, []()`

- `void on ("/logo.png", HTTP_GET, []()`

- `void on ("/reboot", HTTP_GET, []()`

- `void on ("/update", HTTP_POST, []()`

- `void stopFloatOtaAndReboot (void)`

- `void handleFloatOtaMode (void)`

- `void initIMU (void)`

- `void suspendBMI160 (void)`

- `void initTemperatureSensor (void)`

- `void initESPNow (void)`

- `void esp_now_register_recv_cb ([](const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) const`

- `void esp_now_register_recv_cb ([](const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) const`

- `void configureUnusedGPIOs (void)`

- `void ensureDebugSerial (void)`

- `void measureTilt (void)`

- `void measureImuTemperature (void)`

- `void measureTemperature (void)`

- `void calculateDensity (float angle, float temperature)`

- `void calculateSOC (float voltage)`

- `void getBatteryVoltage (void)`

- `void computeSensorData (void)`

- `void transmitData (void)`

- `void onDataAck (const uint8_t *mac, const uint8_t *incomingData, int len) const`

- `void runQuickZeroCalibration (void)`

- `void processQueuedZeroCalibrationCommand (void)`

- `void attachPendingFlagsToPayload (payload_t* payload, bool refreshMeasurement)`

- `void sendPendingZeroCalibrationResult (void)`

- `void storePayloadForRetry (const payload_t& payload) const`

- `void markAcked (uint16_t sequence_id)`

- `void sendPayloadWithAck (payload_t* payload, bool waitForAck)`

- `void sendRetryPayloads (bool includeCurrent)`

- `void enterDeepSleep (void)`

- `void onCalibrationCommand (const uint8_t *mac, const uint8_t *incomingData, int len) const`

- `void recordCalibrationPoint (float target_sg, uint8_t request_id, uint8_t point_number)`

- `void startStableOffsetCalibration (uint8_t request_id)`

- `void handlePendingStableOffsetCalibration (void)`

- `void handlePendingStableCalibrationPoint (void)`

- `void sendCalibrationCoefficients (uint8_t request_id)`

- `void sendCalibrationResponse (float angle, float target_sg, uint8_t request_id, const char* message) const`

- `void sendCalibrationStatus (float etaSeconds, float tempDriftPerMin, uint8_t request_id, const char* message) const`

- `void crc16 (const uint8_t* data, size_t length) const`

- `void printFloatHarnessOK (const char* message) const`

- `void printFloatHarnessERR (const char* message) const`

- `void retryBufferCount (void)`

- `void handleFloatTestHarness (void)`

- `void processFloatHarnessCommand (char* line)`

- `void toggleCalibrationMode (void)`

- `void exitCalibrationMode (void)`

- `void sendCalibrationTrigger (void)`

- `void updateCalibrationActivity (void)`

- `void ema (float prev, float x, float alpha)`

### Enums & Constants

- enum State
  - `INIT`
  - `MEASURE`
  - `COMPUTE`
  - `SEND`
  - `SLEEP`

- enum CalibState
  - `CALIB_IDLE`
  - `CALIB_MEASURING`
  - `CALIB_WAITING_COMMAND`

### Macros

- `#define I2C_SDA 26`

- `#define I2C_SCL 27`

- `#define MEASUREMENT_INTERVAL 600  // seconds (configurable 60-600)`

- `#define SAMPLE_COUNT 5            // samples per measurement (reduced for power efficiency)`

- `#define MAX_RETRIES 1             // transmission retry attempts`

- `#define MAX_INIT_RETRIES 3        // sensor initialization retry attempts`

- `#define BATTERY_PIN 15            // Battery voltage monitoring (GPIO15 - ADC1_CH3)`

- `#define CALIBRATION_SWITCH_PIN 12  // Physical switch for calibration mode (pull-down)`

- `#define ACK_PACKET_TYPE 0xA5`

- `#define RETRY_BUFFER_SIZE 8`

- `#define ACK_WAIT_MS 400`

- `#define SGNODE_FLOAT_TEST_HARNESS 0`

- `#define SGNODE_FLOAT_SERIAL_DIAG 0`

- `#define LED_BUILTIN 16           // Built-in LED - blinks during calibration mode`

- `#define ESPNOW_CHANNEL 1`

- `#define BMI160_ADDR 0x69`

## Key Dependencies

**Local:**
- `SGNode_Float/polynomial_calibration.h`
- `SGNode_Float/../SGNode_Shared/sg_protocol.h`
- `SGNode_Float/../SGNode_Shared/sg_logo_png.h`

**System/External:**
- `<WiFi.h>`
- `<esp_now.h>`
- `<WebServer.h>`
- `<Update.h>`
- `<Wire.h>`
- `<BMI160Gen.h>`

## Side Effects & Hardware Access

- ESP-NOW packet transmission
- ESP-NOW packet reception
- ESP-NOW peer registration
- EEPROM write (persistent storage)
- EEPROM read
- EEPROM initialization
- SD card write (CSV/JSON)
- SD card read
- Sensor reading
- Deep sleep entry
- SPI bus communication
- Temperature target calculation
- Calibration coefficient computation
- Calibration data persistence
- WiFi mode configuration
- WiFi access point mode
- Execution delay
- Serial port initialization
- Serial output

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): 766b927d8caee6c782ad5918f859abfb233d3e6ea66a65b48d30be009dc58318*