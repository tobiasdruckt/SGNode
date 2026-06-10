#pragma once

#include <DallasTemperature.h>
#include <OneWire.h>

#include "plug_types.h"

class PlugSensors {
 public:
  PlugSensors(uint8_t airPin, uint8_t beerPin);

  void begin();
  SensorSnapshot sample();

  static bool isPlausible(float value);

 private:
  OneWire airBus_;
  OneWire beerBus_;
  DallasTemperature airSensor_;
  DallasTemperature beerSensor_;
};
