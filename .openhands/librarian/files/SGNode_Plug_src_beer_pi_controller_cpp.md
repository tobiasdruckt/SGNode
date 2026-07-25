# SGNode_Plug/src/beer_pi_controller.cpp

**Type:** C++ Source
**Hash:** `c48a41b1a2ff77fc068aac006b5c77dcdc2339efcadc2f91e447f0e1bf1d4483`

## Overview / Role

Control logic for temperature or other systems

## Verified API & Symbols

### Functions

- `void clampValue (float value, float minimum, float maximum)`

- `void sanitizeOrDefault (float value, float fallback, float minimum, float maximum)`

- `void sanitizeSettings (const BeerPiSettings& settings) const`

- `void addBeerSample (float beerC, bool valid)`

- `void update (float beerTargetC, float rampKPerHour, float batchLiters, const BeerPiSettings& settings, uint32_t nowMs) const`

- `void reset (void)`

- `void calculateTnHours (float batchLiters)`

## Key Dependencies

**Local:**
- `SGNode_Plug/src/beer_pi_controller.h`

## Side Effects & Hardware Access

- Temperature target calculation

## Change Risks

- **HIGH**: PI control affects temperature regulation

---
*Last modified (hash): c48a41b1a2ff77fc068aac006b5c77dcdc2339efcadc2f91e447f0e1bf1d4483*