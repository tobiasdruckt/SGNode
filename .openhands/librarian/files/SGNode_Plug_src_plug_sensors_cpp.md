# SGNode_Plug/src/plug_sensors.cpp

**Type:** C++ Source
**Hash:** `99a8882c58132acd3b0beeef296839978ab2bd1fa6838c492ed84ef3d28b1c8e`

## Overview / Role

Sensor sampling and validation

## Verified API & Symbols

### Functions

- `void beerSensor_ (&beerBus_)`

- `void begin (void)`

- `void sample (void)`

- `void isPlausible (float value)`

## Key Dependencies

**Local:**
- `SGNode_Plug/src/plug_sensors.h`

## Side Effects & Hardware Access

- Sensor reading

## Change Risks

- **HIGH**: Sensor reading affects data accuracy

---
*Last modified (hash): 99a8882c58132acd3b0beeef296839978ab2bd1fa6838c492ed84ef3d28b1c8e*