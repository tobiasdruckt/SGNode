# SGNode_Base/src/test/ui_test_harness.cpp

**Type:** C++ Source
**Hash:** `5c73a72cf6b9c3f4bcde458ab10a9f16ece2be25f0d1d13208da0426c63e2ceb`

## Overview / Role

User interface components and rendering

## Verified API & Symbols

### Functions

- `void forceRedrawFlags (void)`

- `void printOK (const char* message) const`

- `void printERR (const char* message) const`

- `void skipSpaces (char* text)`

- `void parseQuoted (char* text, char* out, size_t outSize)`

- `void handleCommand (char* line)`

- `void handleUITestHarness (void)`

## Key Dependencies

**Local:**
- `SGNode_Base/src/test/ui_test_harness.h`

**System/External:**
- `<stdio.h>`
- `<stdlib.h>`
- `<string.h>`

## Side Effects & Hardware Access

- SD card read
- Sensor reading
- Touchscreen UI updates
- Serial output

## Change Risks

- **MEDIUM**: UI changes affect user experience

---
*Last modified (hash): 5c73a72cf6b9c3f4bcde458ab10a9f16ece2be25f0d1d13208da0426c63e2ceb*