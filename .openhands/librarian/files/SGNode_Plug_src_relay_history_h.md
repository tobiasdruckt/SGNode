# SGNode_Plug/src/relay_history.h

**Type:** C++ Header
**Hash:** `8673e99bd10d2457277e3c16df54b18294b816088c090e9bc81244ed8f01a6b0`

## Overview / Role

Core functionality module

## Verified API & Symbols

### Structs & Classes

- `class RelayDutyWindow`
  - void recordSecond (bool relayOn)
  - float dutyPercent (void)
  - uint16_t sampleCount (void)

- `class CompressorPattern`
  - void recordMinute (bool relayOn)
  - bool complete (void)
  - void startReplay (void)
  - bool nextReplayMinute (void)
  - uint16_t replayPosition (void)

## Key Dependencies

**System/External:**
- `<Arduino.h>`

## Side Effects & Hardware Access

- No significant side effects

## Change Risks

- **HIGH**: Relay control affects hardware

---
*Last modified (hash): 8673e99bd10d2457277e3c16df54b18294b816088c090e9bc81244ed8f01a6b0*