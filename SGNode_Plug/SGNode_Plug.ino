#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <Updater.h>
#include <espnow.h>
#include <plug_hardware.h>
#include "../SGNode_Shared/sg_logo_png.h"

#include "src/air_controller.h"
#include "src/beer_pi_controller.h"
#include "src/plug_sensors.h"
#include "src/plug_selfcheck.h"
#include "src/plug_transport.h"
#include "src/plug_types.h"
#include "src/relay_history.h"

using namespace SGNodePlugHardware;

constexpr uint32_t SENSOR_INTERVAL_MS = 10000;
constexpr uint32_t DUTY_SAMPLE_INTERVAL_MS = 1000;
constexpr uint32_t PATTERN_INTERVAL_MS = 60000;
constexpr uint32_t STATUS_INTERVAL_MS = 60000;
constexpr uint32_t COMMAND_TIMEOUT_MS = 5UL * 60000UL;
constexpr uint32_t WARMING_TARGET_WINDOW_MS = 24UL * 3600000UL;
constexpr uint32_t OTA_BUTTON_HOLD_MS = 5000;
constexpr uint32_t OTA_WINDOW_MS = 180UL * 1000UL;
constexpr uint32_t OTA_BLINK_INTERVAL_MS = 300;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;

PlugSensors sensors(PIN_AIR_SENSOR, PIN_BEER_SENSOR);
AirController airController;
BeerPiController beerPiController;
RelayDutyWindow relayDuty;
CompressorPattern compressorPattern;
PlugTransport transport;
SensorSnapshot latestSensors;
PlugControlMode controlMode = PlugControlMode::OFF;
uint16_t faults = PLUG_FAULT_NONE;
uint32_t lastSensorMs = 0;
uint32_t lastDutySampleMs = 0;
uint32_t lastPatternMs = 0;
uint32_t lastStatusMs = 0;
float airTargetC = NAN;
float beerTargetC = NAN;
float rampKPerHour = 0.2f;
float batchLiters = 20.0f;
float controllerKp = BeerPiController::DEFAULT_KP;
float controllerTnHours = 0.75f;
float controllerDBrakeHours = BeerPiController::DEFAULT_D_BRAKE_HOURS;
BeerPiSettings beerPiSettings;
AirControllerConfig airControllerConfig;
float beerUndershootLockoutC = 0.1f;
float fastWarmingRateKPerHour = 1.0f;
float targetStepC = 0.25f;
bool actualRelayOn = false;
uint16_t lastPrintedCommandSeq = 0;
float previousBeerTargetC = NAN;
bool warmingTargetActive = false;
uint32_t warmingTargetStartedMs = 0;
ESP8266WebServer otaServer(80);
bool otaModeActive = false;
bool otaUploadInProgress = false;
bool otaUploadOk = false;
uint32_t otaStartedMs = 0;
uint32_t otaLastClientMs = 0;
uint32_t otaRebootAtMs = 0;
uint32_t otaLastBlinkMs = 0;
bool otaBlinkPhase = false;
char otaSsid[32] = "";
bool buttonLastReading = false;
bool buttonStablePressed = false;
bool buttonLongPressFired = false;
uint32_t buttonLastChangeMs = 0;
uint32_t buttonPressStartedMs = 0;

void setActualRelay(bool on);
void printCommandIfChanged();
void updateOtaButton(uint32_t nowMs);
void startOtaMode(uint32_t nowMs);
void handleOtaMode(uint32_t nowMs);
void handleOtaRoot();
void handleOtaDiag();
void handleOtaLogo();
void handleOtaReboot();
void handleOtaUpdateDone();
void handleOtaUpload();
String otaDiagnosticText();

float clampPlugValue(float value, float minimum, float maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

void updateFaults(const SensorSnapshot& sample) {
  faults = PLUG_FAULT_NONE;
  if (!sample.airValid) {
    faults |= PLUG_FAULT_AIR_SENSOR;
  }
  if (!sample.beerValid) {
    faults |= PLUG_FAULT_BEER_SENSOR;
  }
  if (transport.hasCommand() && millis() - transport.lastCommandMs() > COMMAND_TIMEOUT_MS) {
    faults |= PLUG_FAULT_COMMAND_TIMEOUT;
  }
}

uint16_t statusFaultsForCurrentMode() {
  uint16_t statusFaults = faults;
  if (controlMode == PlugControlMode::AIR_ONLY_HOLD &&
      latestSensors.airValid &&
      !latestSensors.beerValid) {
    statusFaults &= ~PLUG_FAULT_BEER_SENSOR;
  }
  return statusFaults;
}

void applyCommandState(uint32_t nowMs) {
  if (!transport.hasCommand()) {
    controlMode = PlugControlMode::OFF;
    setActualRelay(false);
    return;
  }

  const sg_plug_command_t command = transport.command();
  if (isfinite(command.beer_target_c) && isfinite(previousBeerTargetC) &&
      fabsf(command.beer_target_c - previousBeerTargetC) > command.target_step_c) {
    warmingTargetActive = command.beer_target_c > previousBeerTargetC;
    warmingTargetStartedMs = warmingTargetActive ? nowMs : 0;
  }
  if (isfinite(command.beer_target_c)) {
    previousBeerTargetC = command.beer_target_c;
  }
  beerTargetC = command.beer_target_c;
  rampKPerHour = command.ramp_k_per_h;
  batchLiters = command.batch_liters;
  controllerKp = command.controller_kp;
  controllerTnHours = command.controller_tn_h;
  controllerDBrakeHours = command.controller_d_brake_h;
  beerPiSettings.kp = command.controller_kp;
  beerPiSettings.integralTnHours = command.controller_tn_h;
  beerPiSettings.dBrakeHours = command.controller_d_brake_h;
  beerPiSettings.coldIntegralBandC = command.cold_integral_band_c;
  beerPiSettings.warmIntegralBandC = command.warm_integral_band_c;
  beerPiSettings.maxPositiveIntegralC = command.max_positive_integral_c;
  beerPiSettings.maxNegativeIntegralC = command.max_negative_integral_c;
  beerPiSettings.integralLeakPerHour = command.integral_leak_per_hour;
  beerPiSettings.errorCrossingKeepFactor = command.error_crossing_keep_factor;
  beerPiSettings.maxDOffsetC = command.max_d_offset_c;
  beerPiSettings.warmingDFactor = command.warming_d_factor;
  beerPiSettings.strongUndershootC = command.strong_undershoot_c;
  beerPiSettings.strongUndershootAirOffsetC = command.strong_undershoot_air_offset_c;
  beerPiSettings.minAirTargetC = command.min_air_target_c;
  beerPiSettings.maxAirTargetC = command.max_air_target_c;
  beerPiSettings.targetStepC = command.target_step_c;
  beerPiSettings.rampControllerKpHours = command.ramp_controller_kp_h;
  beerPiSettings.rampControllerTnHours = command.ramp_controller_tn_h;
  beerPiSettings.maxRampTrimC = command.max_ramp_trim_c;
  beerPiSettings.rampFadeDistanceC = command.ramp_fade_distance_c;
  beerPiSettings.rateFilterSamples = command.rate_filter_samples;
  airControllerConfig.turnOffAboveTargetC = command.air_turn_off_above_target_c;
  airControllerConfig.turnOnAboveTargetC = command.air_turn_on_above_target_c;
  airControllerConfig.minimumOnMs = (uint32_t)command.air_minimum_on_s * 1000UL;
  airControllerConfig.minimumOffMs = (uint32_t)command.air_minimum_off_s * 1000UL;
  airController.setConfig(airControllerConfig);
  beerUndershootLockoutC = command.beer_undershoot_lockout_c;
  fastWarmingRateKPerHour = command.fast_warming_rate_k_per_h;
  targetStepC = command.target_step_c;

  if (nowMs - transport.lastCommandMs() > COMMAND_TIMEOUT_MS) {
    controlMode = PlugControlMode::OFF;
    faults |= PLUG_FAULT_COMMAND_TIMEOUT;
    setActualRelay(false);
    return;
  }

  if ((command.flags & SG_PLUG_COMMAND_ENABLE) == 0) {
    controlMode = PlugControlMode::OFF;
    setActualRelay(false);
    return;
  }

  if (controlMode == PlugControlMode::OFF || controlMode == PlugControlMode::FAULT) {
    controlMode = PlugControlMode::AUTOMATIC;
  }
}

void sendPlugStatus(uint32_t nowMs) {
  sg_plug_status_v2_t status = {};
  status.command_sequence_id = transport.hasCommand() ? transport.command().sequence_id : 0;
  status.uptime_s = nowMs / 1000;
  status.air_temp_c = latestSensors.airC;
  status.beer_temp_c = latestSensors.beerC;
  status.beer_target_c = beerTargetC;
  status.air_target_c = airTargetC;
  status.duty_10m_percent = relayDuty.dutyPercent();
  status.pi_offset_c = beerPiController.offsetC();
  status.pi_tn_hours = beerPiController.tnHours();
  status.controller_kp = beerPiController.kp();
  status.controller_d_brake_h = beerPiController.dBrakeHours();
  status.beer_rate_c_per_h = beerPiController.beerRateCPerHour();
  status.ramp_k_per_h = rampKPerHour;
  status.control_mode = static_cast<uint8_t>(controlMode);
  status.faults = statusFaultsForCurrentMode();
  status.relay_on = actualRelayOn ? 1 : 0;
  status.pattern_ready = compressorPattern.complete() ? 1 : 0;
  status.p_offset_c = beerPiController.pOffsetC();
  status.i_offset_c = beerPiController.iOffsetC();
  status.d_offset_c = beerPiController.dOffsetC();
  status.ramp_trim_c = beerPiController.rampTrimC();
  status.ramp_i_offset_c = beerPiController.rampITrimC();
  status.ramp_rate_error_k_per_h = beerPiController.rampRateErrorKPerHour();
  status.beer_rate_raw_c_per_h = beerPiController.beerRateRawCPerHour();
  const bool sent = transport.sendStatus(&status);
  Serial.printf("ESP-NOW STATUS %s seq=%u cmd=%u relay=%s mode=%u faults=0x%04X\n",
                sent ? "SENT" : "FAILED", status.sequence_id,
                status.command_sequence_id, actualRelayOn ? "ON" : "OFF",
                static_cast<unsigned>(controlMode), status.faults);
}

void setActualRelay(bool on) {
  actualRelayOn = on;
  setRelay(on);
}

void updateFallbackMode() {
  if (controlMode == PlugControlMode::OFF) {
    return;
  }

  if (!latestSensors.airValid) {
    if (compressorPattern.complete()) {
      if (controlMode != PlugControlMode::TIME_PATTERN_FALLBACK) {
        compressorPattern.startReplay();
      }
      controlMode = PlugControlMode::TIME_PATTERN_FALLBACK;
    } else {
      controlMode = PlugControlMode::FAULT;
      faults |= PLUG_FAULT_NO_PATTERN;
      setActualRelay(false);
    }
  } else if (!latestSensors.beerValid) {
    controlMode = PlugControlMode::AIR_ONLY_HOLD;
  } else if (controlMode == PlugControlMode::AIR_ONLY_HOLD ||
             controlMode == PlugControlMode::TIME_PATTERN_FALLBACK) {
    controlMode = PlugControlMode::AUTOMATIC;
  }
}

void printStatus() {
  Serial.printf("AIR %.2f (%s) | BEER %.2f (%s) | AIR TARGET %.2f | RELAY %s | DUTY10 %.1f%% | PI off %.2f P %.2f Tn %.1f D %.1f rate %.2fK/h | PATTERN %s | MODE %u | FAULTS 0x%04X\n",
                latestSensors.airC, latestSensors.airValid ? "OK" : "ERR",
                latestSensors.beerC, latestSensors.beerValid ? "OK" : "ERR",
                airTargetC, actualRelayOn ? "ON" : "OFF", relayDuty.dutyPercent(),
                beerPiController.offsetC(), beerPiController.kp(), beerPiController.tnHours(),
                beerPiController.dBrakeHours(), beerPiController.beerRateCPerHour(),
                compressorPattern.complete() ? "READY" : "LEARNING",
                static_cast<unsigned>(controlMode), statusFaultsForCurrentMode());
}

void printCommandIfChanged() {
  if (!transport.hasCommand()) {
    return;
  }

  const sg_plug_command_t command = transport.command();
  if (command.sequence_id == lastPrintedCommandSeq) {
    return;
  }
  lastPrintedCommandSeq = command.sequence_id;
  Serial.printf("ESP-NOW COMMAND seq=%u flags=0x%02X target=%.2fC ramp=%.2fK/h batch=%.1fL P=%.2f I=%.2fh D=%.1fh airH=%.1f/%.1f\n",
                command.sequence_id, command.flags, command.beer_target_c,
                command.ramp_k_per_h, command.batch_liters,
                command.controller_kp, command.controller_tn_h,
                command.controller_d_brake_h,
                command.air_turn_off_above_target_c,
                command.air_turn_on_above_target_c);
}

void updateOtaButton(uint32_t nowMs) {
  const bool pressedReading = digitalRead(PIN_BUTTON) == LOW;
  if (pressedReading != buttonLastReading) {
    buttonLastReading = pressedReading;
    buttonLastChangeMs = nowMs;
  }

  if (nowMs - buttonLastChangeMs < BUTTON_DEBOUNCE_MS) {
    return;
  }

  if (pressedReading != buttonStablePressed) {
    buttonStablePressed = pressedReading;
    if (buttonStablePressed) {
      buttonPressStartedMs = nowMs;
      buttonLongPressFired = false;
    } else {
      buttonPressStartedMs = 0;
      buttonLongPressFired = false;
    }
  }

  if (buttonStablePressed && !buttonLongPressFired &&
      nowMs - buttonPressStartedMs >= OTA_BUTTON_HOLD_MS) {
    buttonLongPressFired = true;
    if (otaModeActive) {
      otaRebootAtMs = nowMs + 100UL;
    } else {
      startOtaMode(nowMs);
    }
  }
}

String otaDiagnosticText() {
  String text;
  text.reserve(700);
  const uint32_t nowMs = millis();
  const uint32_t remainingS = otaLastClientMs + OTA_WINDOW_MS > nowMs
    ? (otaLastClientMs + OTA_WINDOW_MS - nowMs) / 1000UL
    : 0;
  const uint8_t clients = WiFi.softAPgetStationNum();
  text += F("Firmware: ");
  text += F(__DATE__);
  text += ' ';
  text += F(__TIME__);
  text += '\n';
  text += F("Chip ID: ");
  text += String(ESP.getChipId(), HEX);
  text += '\n';
  text += F("Flash real/sketch/free: ");
  text += ESP.getFlashChipRealSize();
  text += '/';
  text += ESP.getSketchSize();
  text += '/';
  text += ESP.getFreeSketchSpace();
  text += '\n';
  text += F("Free heap: ");
  text += ESP.getFreeHeap();
  text += '\n';
  text += F("Uptime s: ");
  text += nowMs / 1000UL;
  text += '\n';
  text += F("Reset: ");
  text += ESP.getResetReason();
  text += '\n';
  text += F("AP: ");
  text += otaSsid;
  text += F(" IP ");
  text += WiFi.softAPIP().toString();
  text += '\n';
  text += F("OTA idle remaining s: ");
  text += clients > 0 ? F("client connected") : String(remainingS);
  text += '\n';
  text += F("AP clients: ");
  text += clients;
  text += '\n';
  text += F("Relay: ");
  text += actualRelayOn ? F("on") : F("off");
  text += '\n';
  text += F("Mode: ");
  text += static_cast<unsigned>(controlMode);
  text += '\n';
  text += F("Faults: 0x");
  char hexBuf[8];
  snprintf(hexBuf, sizeof(hexBuf), "%04X", statusFaultsForCurrentMode());
  text += hexBuf;
  text += '\n';
  text += F("Air: ");
  text += latestSensors.airC;
  text += latestSensors.airValid ? F(" valid") : F(" invalid");
  text += '\n';
  text += F("Beer: ");
  text += latestSensors.beerC;
  text += latestSensors.beerValid ? F(" valid") : F(" invalid");
  text += '\n';
  text += F("Last command age s: ");
  text += transport.hasCommand() ? String((nowMs - transport.lastCommandMs()) / 1000UL) : F("none");
  text += '\n';
  text += F("ESP-NOW channel: 1");
  text += '\n';
  return text;
}

void handleOtaRoot() {
  String page;
  page.reserve(1500);
  page += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>SGNode Plug OTA</title><style>body{font-family:sans-serif;margin:24px;max-width:720px}");
  page += F("button,input{font-size:16px;padding:10px;margin:6px 0}pre{background:#f3f4f6;padding:12px;overflow:auto}");
  page += F(".hdr{display:flex;align-items:center;gap:14px;margin-bottom:16px}.logo{width:96px;height:auto}</style></head><body>");
  page += F("<div class='hdr'><img class='logo' src='/logo.png' alt='SGNode'><h2>Plug OTA</h2></div>");
  page += F("<form method='POST' action='/update' enctype='multipart/form-data'>");
  page += F("<input type='file' name='firmware'><input type='submit' value='Upload'></form>");
  page += F("<form method='POST' action='/reboot'><input type='submit' value='Reboot normal'></form>");
  page += F("<p><a href='/diag'>diag</a></p><pre>");
  page += otaDiagnosticText();
  page += F("</pre></body></html>");
  otaServer.send(200, F("text/html"), page);
}

void handleOtaDiag() {
  otaServer.send(200, F("text/plain"), otaDiagnosticText());
}

void handleOtaLogo() {
  otaServer.send_P(200, "image/png", (const char*)SGNODE_LOGO_PNG, SGNODE_LOGO_PNG_SIZE);
}

void handleOtaReboot() {
  otaServer.send(200, F("text/plain"), F("Rebooting to normal mode"));
  otaRebootAtMs = millis() + 500UL;
}

void handleOtaUpdateDone() {
  const bool ok = !Update.hasError() && otaUploadOk;
  otaServer.send(ok ? 200 : 500, F("text/plain"), ok ? F("OK rebooting") : F("Update failed"));
  otaRebootAtMs = millis() + 1000UL;
}

void handleOtaUpload() {
  HTTPUpload& upload = otaServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaUploadInProgress = true;
    otaUploadOk = false;
    setActualRelay(false);
    const uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    otaUploadOk = Update.end(true);
    if (!otaUploadOk) {
      Update.printError(Serial);
    }
    otaUploadInProgress = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    otaUploadInProgress = false;
    otaUploadOk = false;
  }
}

void startOtaMode(uint32_t nowMs) {
  if (otaModeActive) return;
  otaModeActive = true;
  otaStartedMs = nowMs;
  otaLastClientMs = nowMs;
  otaUploadInProgress = false;
  otaUploadOk = false;
  otaRebootAtMs = 0;
  setActualRelay(false);
  controlMode = PlugControlMode::OFF;

  esp_now_deinit();
  WiFi.persistent(false);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  snprintf(otaSsid, sizeof(otaSsid), "SGNode-Plug-OTA-%06X", ESP.getChipId());
  WiFi.softAP(otaSsid);

  pinMode(PIN_LED_PRIMARY, OUTPUT);
  pinMode(PIN_LED_SECONDARY, OUTPUT);
  digitalWrite(PIN_LED_PRIMARY, HIGH);
  digitalWrite(PIN_LED_SECONDARY, HIGH);

  otaServer.on(F("/"), HTTP_GET, handleOtaRoot);
  otaServer.on(F("/diag"), HTTP_GET, handleOtaDiag);
  otaServer.on(F("/logo.png"), HTTP_GET, handleOtaLogo);
  otaServer.on(F("/reboot"), HTTP_POST, handleOtaReboot);
  otaServer.on(F("/update"), HTTP_GET, handleOtaRoot);
  otaServer.on(F("/update"), HTTP_POST, handleOtaUpdateDone, handleOtaUpload);
  otaServer.begin();
  Serial.printf("OTA mode active SSID=%s IP=%s\n", otaSsid, WiFi.softAPIP().toString().c_str());
}

void handleOtaMode(uint32_t nowMs) {
  setActualRelay(false);
  otaServer.handleClient();
  if (WiFi.softAPgetStationNum() > 0) {
    otaLastClientMs = nowMs;
  }

  if (nowMs - otaLastBlinkMs >= OTA_BLINK_INTERVAL_MS) {
    otaLastBlinkMs = nowMs;
    otaBlinkPhase = !otaBlinkPhase;
    digitalWrite(PIN_LED_PRIMARY, otaBlinkPhase ? LOW : HIGH);
    digitalWrite(PIN_LED_SECONDARY, otaBlinkPhase ? HIGH : LOW);
  }

  if (otaRebootAtMs > 0 && nowMs >= otaRebootAtMs) {
    ESP.restart();
  }
  if (!otaUploadInProgress && WiFi.softAPgetStationNum() == 0 &&
      nowMs - otaLastClientMs >= OTA_WINDOW_MS) {
    WiFi.softAPdisconnect(true);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  beginRelayOff();
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED_PRIMARY, OUTPUT);
  digitalWrite(PIN_LED_PRIMARY, HIGH);
  sensors.begin();
  const bool transportReady = transport.begin();

  Serial.println();
  Serial.println(F("SGNode Plug firmware"));
  runPlugSelfChecks(Serial);
  Serial.println(F("SELF-CHECK PASS: hardware relay initialized OFF"));
  Serial.printf("ESP-NOW transport: %s\n", transportReady ? "READY" : "ERROR");
}

void loop() {
  const uint32_t nowMs = millis();
  if (!otaModeActive) {
    updateOtaButton(nowMs);
  }
  if (otaModeActive) {
    handleOtaMode(nowMs);
    return;
  }

  applyCommandState(nowMs);
  printCommandIfChanged();

  if (nowMs - lastDutySampleMs >= DUTY_SAMPLE_INTERVAL_MS) {
    lastDutySampleMs = nowMs;
    relayDuty.recordSecond(actualRelayOn);
  }

  if (nowMs - lastPatternMs >= PATTERN_INTERVAL_MS) {
    lastPatternMs = nowMs;
    if ((controlMode == PlugControlMode::AUTOMATIC ||
         controlMode == PlugControlMode::AIR_ONLY_HOLD) &&
        latestSensors.airValid) {
      compressorPattern.recordMinute(actualRelayOn);
    } else if (controlMode == PlugControlMode::TIME_PATTERN_FALLBACK) {
      setActualRelay(compressorPattern.nextReplayMinute());
    }
  }

  if (nowMs - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = nowMs;
    sendPlugStatus(nowMs);
  }

  if (nowMs - lastSensorMs < SENSOR_INTERVAL_MS) {
    return;
  }
  lastSensorMs = nowMs;

  latestSensors = sensors.sample();
  updateFaults(latestSensors);
  updateFallbackMode();
  beerPiController.addBeerSample(latestSensors.beerC, latestSensors.beerValid);
  if (controlMode == PlugControlMode::AUTOMATIC && latestSensors.beerValid) {
    airTargetC = beerPiController.update(beerTargetC, rampKPerHour, batchLiters,
                                         beerPiSettings, nowMs);
  } else if (controlMode == PlugControlMode::AIR_ONLY_HOLD) {
    beerPiController.reset();
    airTargetC = isfinite(beerTargetC)
      ? clampPlugValue(beerTargetC, beerPiSettings.minAirTargetC, beerPiSettings.maxAirTargetC)
      : NAN;
  } else if (controlMode == PlugControlMode::OFF || controlMode == PlugControlMode::FAULT) {
    beerPiController.reset();
    airTargetC = NAN;
  }
  const bool controllerEnabled = controlMode == PlugControlMode::AUTOMATIC ||
                                 controlMode == PlugControlMode::AIR_ONLY_HOLD;
  if (warmingTargetActive &&
      (!latestSensors.beerValid ||
       latestSensors.beerC >= beerTargetC - beerUndershootLockoutC ||
       nowMs - warmingTargetStartedMs > WARMING_TARGET_WINDOW_MS)) {
    warmingTargetActive = false;
  }
  const bool allowCoolingDuringFastWarmRamp =
    warmingTargetActive &&
    latestSensors.beerValid &&
    beerPiController.beerRateCPerHour() > fastWarmingRateKPerHour;
  const bool compressorLockedOutByBeerUndershoot =
    controlMode == PlugControlMode::AUTOMATIC &&
    latestSensors.beerValid &&
    isfinite(beerTargetC) &&
    latestSensors.beerC < beerTargetC - beerUndershootLockoutC &&
    !allowCoolingDuringFastWarmRamp;
  const bool relayOn = airController.update(latestSensors.airC, latestSensors.airValid,
                                             airTargetC,
                                             controllerEnabled && !compressorLockedOutByBeerUndershoot,
                                             nowMs);
  if (controlMode != PlugControlMode::TIME_PATTERN_FALLBACK) {
    setActualRelay(relayOn);
  }
  printStatus();
}
