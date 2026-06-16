#include "plug_selfcheck.h"

#include "air_controller.h"
#include "beer_pi_controller.h"
#include "relay_history.h"

namespace {

bool check(Print& output, bool condition, const __FlashStringHelper* name) {
  output.print(condition ? F("SELF-CHECK PASS: ") : F("SELF-CHECK FAIL: "));
  output.println(name);
  return condition;
}

}  // namespace

bool runPlugSelfChecks(Print& output) {
  AirControllerConfig config;
  config.turnOffAboveTargetC = 0.5f;
  config.turnOnAboveTargetC = 1.1f;
  config.minimumOnMs = 0;
  config.minimumOffMs = 0;
  AirController controller(config);

  bool passed = true;
  passed &= check(output, !controller.update(21.0f, true, 20.0f, true, 1),
                  F("air controller holds below raised on threshold"));
  passed &= check(output, controller.update(21.2f, true, 20.0f, true, 2),
                  F("air controller switches on above raised on threshold"));
  passed &= check(output, controller.update(20.7f, true, 20.0f, true, 3),
                  F("air controller holds above raised off threshold"));
  passed &= check(output, !controller.update(20.4f, true, 20.0f, true, 4),
                  F("air controller switches off before aftercool"));
  passed &= check(output, !controller.update(25.0f, false, 20.0f, true, 5),
                  F("invalid air sensor forces relay off"));

  RelayDutyWindow duty;
  for (uint16_t i = 0; i < RelayDutyWindow::WINDOW_SECONDS; ++i) {
    duty.recordSecond((i % 2) == 0);
  }
  passed &= check(output, duty.sampleCount() == RelayDutyWindow::WINDOW_SECONDS &&
                              fabsf(duty.dutyPercent() - 50.0f) < 0.01f,
                  F("10 minute duty window"));

  CompressorPattern pattern;
  for (uint16_t i = 0; i < CompressorPattern::PATTERN_MINUTES; ++i) {
    pattern.recordMinute(i == 0);
  }
  pattern.startReplay();
  passed &= check(output, pattern.complete() && pattern.nextReplayMinute(),
                  F("complete 6 hour pattern replays oldest minute"));

  passed &= check(output, fabsf(BeerPiController::calculateTnHours(13.0f) - 10.0f) < 0.01f &&
                              BeerPiController::calculateTnHours(1.0f) == 6.0f &&
                              BeerPiController::calculateTnHours(40.0f) == 24.0f,
                  F("PI Tn follows batch size with 6h/24h clamps"));

  BeerPiController pi;
  BeerPiSettings piSettings;
  piSettings.kp = 0.5f;
  piSettings.integralTnHours = 0.75f;
  piSettings.dBrakeHours = 0.0f;
  for (uint8_t i = 0; i < BeerPiController::AVERAGE_SAMPLES; ++i) {
    pi.addBeerSample(23.0f, true);
  }
  const float coolingAirTarget = pi.update(20.0f, 0.0f, 13.0f, piSettings, 1);
  passed &= check(output, fabsf(coolingAirTarget - 18.5f) < 0.01f,
                  F("PI lowers air target when beer is warm"));

  pi.reset();
  pi.addBeerSample(10.0f, true);
  const float warmupAirTarget = pi.update(12.0f, 0.0f, 13.0f, piSettings, 1);
  passed &= check(output, fabsf(warmupAirTarget - 13.8f) < 0.01f,
                  F("PI raises air target when beer is cold"));

  pi.reset();
  pi.addBeerSample(10.0f, true);
  piSettings.kp = 1.0f;
  const float safetyAirTarget = pi.update(2.5f, 1.0f, 13.0f, piSettings, 1);
  passed &= check(output, fabsf(safetyAirTarget - 1.0f) < 0.01f,
                  F("PI air target respects 1 C safety minimum"));
  return passed;
}
