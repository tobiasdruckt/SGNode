# SGNode_Plug/src/beer_pi_controller.h

**Type:** C++ Header
**Hash:** `788747aae890c235e9dc87a8beb9a4ea898bded5d79dc93e7830d6da0caa8d65`

## Role

PI temperature controller with D-brake for compressor control.

## Verified API / Symbols

- Structs: BeerPiSettings
- Classes: BeerPiController

- Functions: addBeerSample, update, reset, beerAverageC, beerControlC, beerRateCPerHour, airTargetC, offsetC, tnHours, kp

## Key Dependencies

- Arduino.h


## Side Effects

- No significant side effects

## Tests

- No dedicated automated test identified

## Change Risks

- **HIGH**: PI control affects temperature regulation

---
*Last modified (hash): 788747aae890c235e9dc87a8beb9a4ea898bded5d79dc93e7830d6da0caa8d65*