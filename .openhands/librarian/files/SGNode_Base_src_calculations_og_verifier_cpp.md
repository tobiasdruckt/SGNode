# SGNode_Base/src/calculations/og_verifier.cpp

**Type:** C++ Source
**Hash:** `0afec698748e82e2cec90c1a277b31e3fc56c10c3e2ed2a3715a84d90f9dc3aa`

## Overview / Role

Mathematical calculations for brewing metrics

## Verified API & Symbols

### Functions

- `void OGVerifier (void)`

- `void reset (void)`

- `void addReading (float sg)`

- `void exceedsThreshold (float difference)`

## Key Dependencies

**Local:**
- `SGNode_Base/src/calculations/og_verifier.h`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Calculation changes affect data accuracy

---
*Last modified (hash): 0afec698748e82e2cec90c1a277b31e3fc56c10c3e2ed2a3715a84d90f9dc3aa*