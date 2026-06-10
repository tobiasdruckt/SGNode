#include <DallasTemperature.h>
#include <OneWire.h>
#include <plug_hardware.h>

using namespace SGNodePlugHardware;

constexpr float RELAY_TEST_HYSTERESIS_C = 0.20f;
constexpr uint32_t SAMPLE_INTERVAL_MS = 2000;

OneWire airBus(PIN_AIR_SENSOR);
OneWire beerBus(PIN_BEER_SENSOR);
DallasTemperature airSensor(&airBus);
DallasTemperature beerSensor(&beerBus);

bool relayOn = false;
uint32_t lastSampleMs = 0;

bool isPlausible(float value) {
  return value != DEVICE_DISCONNECTED_C && value > -20.0f && value < 60.0f;
}

void updateRelay(float airC, float beerC) {
  if (!isPlausible(airC) || !isPlausible(beerC)) {
    relayOn = false;
  } else if (beerC > airC + RELAY_TEST_HYSTERESIS_C) {
    relayOn = true;
  } else if (airC > beerC + RELAY_TEST_HYSTERESIS_C) {
    relayOn = false;
  }

  setRelay(relayOn);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  beginRelayOff();
  airSensor.begin();
  beerSensor.begin();
  airSensor.setResolution(12);
  beerSensor.setResolution(12);

  Serial.println();
  Serial.println(F("SGNode Plug dual DS18B20 / relay bench test"));
  Serial.println(F("BEER > AIR + 0.20 C: relay ON"));
  Serial.println(F("AIR > BEER + 0.20 C: relay OFF"));
  Serial.println(F("Invalid sensor: relay OFF"));
}

void loop() {
  if (millis() - lastSampleMs < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleMs = millis();

  airSensor.requestTemperatures();
  beerSensor.requestTemperatures();
  const float airC = airSensor.getTempCByIndex(0);
  const float beerC = beerSensor.getTempCByIndex(0);
  updateRelay(airC, beerC);

  Serial.printf("AIR %.2f C | BEER %.2f C | RELAY %s\n",
                airC, beerC, relayOn ? "ON" : "OFF");
}
