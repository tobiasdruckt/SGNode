# SGNode_Plug/src/beer_pi_controller.h

**Type:** C++ Header
**Hash:** `80c80c35ad307ca0777c2a7cdc5647b42fa00c524eb49c43914abe0a8b6b7383`

## Overview / Role

Control logic for temperature or other systems

## Verified API & Symbols

### Structs & Classes

- `struct BeerPiSettings`
  - `float kp = 0.45f`
  - `float integralTnHours = 0.75f`
  - `float dBrakeHours = 0.8f`
  - `float coldIntegralBandC = 1.0f`
  - `float warmIntegralBandC = 0.5f`
  - `float maxPositiveIntegralC = 2.0f`
  - `float maxNegativeIntegralC = -0.6f`
  - `float integralLeakPerHour = 1.0f`
  - `float errorCrossingKeepFactor = 0.25f`
  - `float maxDOffsetC = 0.9f`
  - `float warmingDFactor = 0.25f`
  - `float strongUndershootC = 1.0f`
  - `float strongUndershootAirOffsetC = 1.8f`
  - `float minAirTargetC = 1.0f`
  - `float maxAirTargetC = 30.0f`
  - `float targetStepC = 0.25f`
  - `float rampControllerKpHours = 0.8f`
  - `float rampControllerTnHours = 3.0f`
  - `float maxRampTrimC = 1.0f`
  - `float rampFadeDistanceC = 1.0f`
  - `float rateFilterSamples = 5.0f`

- `class BeerPiController`
  - void addBeerSample (float beerC, bool valid)
  - void reset (void)
  - float beerAverageC (void)
  - float beerControlC (void)
  - float beerRateCPerHour (void)
  - float beerRateRawCPerHour (void)
  - float airTargetC (void)
  - float offsetC (void)
  - float pOffsetC (void)
  - float iOffsetC (void)
  - float dOffsetC (void)
  - float rampTrimC (void)
  - float rampITrimC (void)
  - float rampRateErrorKPerHour (void)
  - float tnHours (void)
  - float kp (void)
  - float dBrakeHours (void)
  - float calculateTnHours (float batchLiters) static
  - [private]
  - float calculateBeerRateCPerHour (void)

## Key Dependencies

**System/External:**
- `<Arduino.h>`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **HIGH**: PI control affects temperature regulation

---
*Last modified (hash): 80c80c35ad307ca0777c2a7cdc5647b42fa00c524eb49c43914abe0a8b6b7383*