# SGNode_Base/src/calculations/derived_calculations.cpp

**Type:** C++ Source
**Hash:** `243895a102bf4479953156a882fa150367b92e130b9acc739585d031d84e51b9`

## Overview / Role

Mathematical calculations for brewing metrics

## Verified API & Symbols

### Functions

- `void expectedFG (float og, int apparentAttenuationPercent)`

- `void apparentAttenuation (float effectiveOG, float currentSG)`

- `void abv (float effectiveOG, float currentSG)`

- `void gravityDeltaPerHour (float previousSG, float currentSG, unsigned long deltaSeconds)`

- `void compute (float effectiveOG, float currentSG, float previousSG, unsigned long deltaSeconds, int attenuationPercent)`

## Key Dependencies

**Local:**
- `SGNode_Base/src/calculations/derived_calculations.h`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Calculation changes affect data accuracy

---
*Last modified (hash): 243895a102bf4479953156a882fa150367b92e130b9acc739585d031d84e51b9*