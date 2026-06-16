#include <plug_hardware.h>

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

void setActualRelay(bool on);
void printCommandIfChanged();

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
  sg_plug_status_t status = {};
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
  status.faults = faults;
  status.relay_on = actualRelayOn ? 1 : 0;
  status.pattern_ready = compressorPattern.complete() ? 1 : 0;
  const bool sent = transport.sendStatus(&status);
  Serial.printf("ESP-NOW STATUS %s seq=%u cmd=%u relay=%s mode=%u faults=0x%04X\n",
                sent ? "SENT" : "FAILED", status.sequence_id,
                status.command_sequence_id, actualRelayOn ? "ON" : "OFF",
                static_cast<unsigned>(controlMode), faults);
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
                static_cast<unsigned>(controlMode), faults);
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

void setup() {
  Serial.begin(SERIAL_BAUD);
  beginRelayOff();
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
