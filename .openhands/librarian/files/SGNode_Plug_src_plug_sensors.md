# SGNode_Plug/src/plug_sensors.h

**Type:** C++ Header
**Hash:** `9586259b4250ebbc508dac38c921defb7582a50385e0e59b7f74acd24a8b07a7`

## Role

DS18B20 temperature sensor reading with CRC validation.

## Verified API / Symbols

- Classes: PlugSensors

- Functions: begin, isPlausible

## Key Dependencies

- Arduino.h
- OneWire.h
- DallasTemperature.h (for beer sensor)


## Side Effects

- I2C/SPI bus communication

## Tests

- No dedicated automated test identified

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): 9586259b4250ebbc508dac38c921defb7582a50385e0e59b7f74acd24a8b07a7*