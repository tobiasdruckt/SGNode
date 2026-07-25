# SGNode_Base/src/calculations/eta_predictor.cpp

**Type:** C++ Source
**Hash:** `9147caf6f515595bcf5204c36f570b9289767c598bb90dfb864a7da802da440c`

## Overview / Role

Mathematical calculations for brewing metrics

## Verified API & Symbols

### Functions

- `void predict (float currentSG, float expectedFG, float gravityDeltaPerHour)`

- `void predict (const BrewProfile& profile, float currentSG, float expectedFG, float gravityDeltaPerHour, unsigned long elapsedSeconds) const`

## Key Dependencies

**Local:**
- `SGNode_Base/src/calculations/eta_predictor.h`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Calculation changes affect data accuracy

---
*Last modified (hash): 9147caf6f515595bcf5204c36f570b9289767c598bb90dfb864a7da802da440c*