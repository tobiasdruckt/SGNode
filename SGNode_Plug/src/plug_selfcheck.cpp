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
  config.hysteresisC = 0.8f;
  config.minimumOnMs = 0;
  config.minimumOffMs = 0;
  AirController controller(config);

  bool passed = true;
  passed &= check(output, !controller.update(20.3f, true, 20.0f, true, 1),
                  F("air controller holds inside upper band"));
  passed &= check(output, controller.update(20.5f, true, 20.0f, true, 2),
                  F("air controller switches on above upper band"));
  passed &= check(output, controller.update(19.7f, true, 20.0f, true, 3),
                  F("air controller holds inside lower band"));
  passed &= check(output, !controller.update(19.5f, true, 20.0f, true, 4),
                  F("air controller switches off below lower band"));
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
  for (uint8_t i = 0; i < BeerPiController::AVERAGE_SAMPLES; ++i) {
    pi.addBeerSample(23.0f, true);
  }
  const float holdAirTarget = pi.update(20.0f, 0.0f, 13.0f, 1);
  passed &= check(output, fabsf(holdAirTarget - 17.0f) < 0.01f,
                  F("PI static offset clamps at 3 K"));

  pi.reset();
  pi.addBeerSample(10.0f, true);
  const float fastAirTarget = pi.update(2.5f, 1.0f, 13.0f, 1);
  passed &= check(output, fabsf(fastAirTarget - 1.0f) < 0.01f,
                  F("PI fast-ramp air target respects 1 C minimum"));
  return passed;
}
