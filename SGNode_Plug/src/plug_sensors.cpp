#include "plug_sensors.h"

PlugSensors::PlugSensors(uint8_t airPin, uint8_t beerPin)
    : airBus_(airPin),
      beerBus_(beerPin),
      airSensor_(&airBus_),
      beerSensor_(&beerBus_) {}

void PlugSensors::begin() {
  airSensor_.begin();
  beerSensor_.begin();
  airSensor_.setResolution(12);
  beerSensor_.setResolution(12);
}

SensorSnapshot PlugSensors::sample() {
  airSensor_.requestTemperatures();
  beerSensor_.requestTemperatures();

  SensorSnapshot result;
  result.airC = airSensor_.getTempCByIndex(0);
  result.beerC = beerSensor_.getTempCByIndex(0);
  result.airValid = isPlausible(result.airC);
  result.beerValid = isPlausible(result.beerC);
  result.sampledAtMs = millis();
  return result;
}

bool PlugSensors::isPlausible(float value) {
  return isfinite(value) && value != DEVICE_DISCONNECTED_C &&
         value > -20.0f && value < 60.0f;
}
