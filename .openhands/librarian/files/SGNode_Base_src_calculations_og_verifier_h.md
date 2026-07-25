# SGNode_Base/src/calculations/og_verifier.h

**Type:** C++ Header
**Hash:** `7cd8842a7dc2c6c17055c48ca72b5647b7033b5a860d16baebd26bbf43b53515`

## Overview / Role

Mathematical calculations for brewing metrics

## Verified API & Symbols

### Structs & Classes

- `struct OGVerificationResult`
  - `bool ready`
  - `bool needsUserChoice`
  - `float measuredOG`
  - `float ogDifference`

- `class OGVerifier`
  - OGVerifie r (void)
  - void reset (void)
  - OGVerificationResult addReading (float sg)
  - bool exceedsThreshold (float difference) static

### Macros

- `#define OG_VERIFIER_H struct OGVerificationResult {`

## Key Dependencies

- No external dependencies

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **MEDIUM**: Calculation changes affect data accuracy

---
*Last modified (hash): 7cd8842a7dc2c6c17055c48ca72b5647b7033b5a860d16baebd26bbf43b53515*