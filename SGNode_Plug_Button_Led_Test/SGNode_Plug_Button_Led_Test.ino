#include <plug_hardware.h>

using namespace SGNodePlugHardware;

constexpr uint32_t BLINK_INTERVAL_MS = 400;
constexpr uint32_t DEBOUNCE_MS = 40;

bool blinking = true;
bool ledOn = false;
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
uint32_t lastButtonChangeMs = 0;
uint32_t lastBlinkMs = 0;

void setLedPair(bool on) {
  // Gosund SP1 LEDs are inverted in the known template.
  digitalWrite(PIN_LED_PRIMARY, on ? LOW : HIGH);
  digitalWrite(PIN_LED_SECONDARY, on ? LOW : HIGH);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  beginRelayOff();

  pinMode(PIN_LED_PRIMARY, OUTPUT);
  pinMode(PIN_LED_SECONDARY, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  setLedPair(false);

  Serial.println();
  Serial.println(F("SGNode Plug button/LED test"));
  Serial.println(F("Press button: toggle blinking"));
}

void loop() {
  const uint32_t nowMs = millis();
  const bool reading = digitalRead(PIN_BUTTON);

  if (reading != lastButtonReading) {
    lastButtonChangeMs = nowMs;
    lastButtonReading = reading;
  }

  if (nowMs - lastButtonChangeMs >= DEBOUNCE_MS &&
      reading != stableButtonState) {
    stableButtonState = reading;
    if (stableButtonState == LOW) {
      blinking = !blinking;
      Serial.printf("Blinking %s\n", blinking ? "ON" : "OFF");
      if (!blinking) {
        ledOn = false;
        setLedPair(false);
      }
    }
  }

  if (blinking && nowMs - lastBlinkMs >= BLINK_INTERVAL_MS) {
    lastBlinkMs = nowMs;
    ledOn = !ledOn;
    setLedPair(ledOn);
  }
}
