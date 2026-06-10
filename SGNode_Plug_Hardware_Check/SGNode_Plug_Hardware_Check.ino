#include <plug_hardware.h>

using namespace SGNodePlugHardware;

void setup() {
  Serial.begin(SERIAL_BAUD);
  beginRelayOff();

  Serial.println();
  Serial.println(F("SGNode Plug hardware definition"));
  Serial.printf("Air DS18B20: GPIO%u\n", PIN_AIR_SENSOR);
  Serial.printf("Beer DS18B20: GPIO%u\n", PIN_BEER_SENSOR);
  Serial.printf("Relay: GPIO%u, active high, initialized OFF\n", PIN_RELAY);
}

void loop() {
  delay(1000);
}
