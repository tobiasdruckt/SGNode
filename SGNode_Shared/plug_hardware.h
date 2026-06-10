#pragma once

#include <Arduino.h>

namespace SGNodePlugHardware {

constexpr uint8_t PIN_LED_SECONDARY = 1;
constexpr uint8_t PIN_BUTTON = 3;
constexpr uint8_t PIN_AIR_SENSOR = 4;
constexpr uint8_t PIN_BEER_SENSOR = 5;
constexpr uint8_t PIN_LED_PRIMARY = 13;
constexpr uint8_t PIN_RELAY = 14;

constexpr uint8_t RELAY_ON_LEVEL = HIGH;
constexpr uint8_t RELAY_OFF_LEVEL = LOW;

constexpr uint32_t SERIAL_BAUD = 115200;

static_assert(PIN_AIR_SENSOR != PIN_BEER_SENSOR, "Each DS18B20 needs its own OneWire bus");
static_assert(PIN_RELAY != PIN_AIR_SENSOR, "Relay and air sensor pin collide");
static_assert(PIN_RELAY != PIN_BEER_SENSOR, "Relay and beer sensor pin collide");
static_assert(PIN_BUTTON != PIN_AIR_SENSOR, "Button and air sensor pin collide");
static_assert(PIN_BUTTON != PIN_BEER_SENSOR, "Button and beer sensor pin collide");
static_assert(RELAY_ON_LEVEL == HIGH, "Gosund SP1 relay must be active high");

inline void beginRelayOff() {
  digitalWrite(PIN_RELAY, RELAY_OFF_LEVEL);
  pinMode(PIN_RELAY, OUTPUT);
}

inline void setRelay(bool on) {
  digitalWrite(PIN_RELAY, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

}  // namespace SGNodePlugHardware
