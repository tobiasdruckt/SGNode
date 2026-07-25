# SGNode_Plug/src/plug_sensors.h

**Type:** C++ Header
**Hash:** `9586259b4250ebbc508dac38c921defb7582a50385e0e59b7f74acd24a8b07a7`

## Overview / Role

Sensor sampling and validation

## Verified API & Symbols

### Structs & Classes

- `class PlugSensors`
  - PlugSensor s (uint8_t airPin, uint8_t beerPin)
  - void begin (void)
  - SensorSnapshot sample (void)
  - bool isPlausible (float value) static

## Key Dependencies

**Local:**
- `SGNode_Plug/src/plug_types.h`

**System/External:**
- `<DallasTemperature.h>`
- `<OneWire.h>`

## Side Effects & Hardware Access

- Sensor reading
- I2C bus communication
- OneWire bus communication

## Change Risks

- **HIGH**: Sensor reading affects data accuracy

---
*Last modified (hash): 9586259b4250ebbc508dac38c921defb7582a50385e0e59b7f74acd24a8b07a7*