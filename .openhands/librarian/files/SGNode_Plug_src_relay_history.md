# SGNode_Plug/src/relay_history.h

**Type:** C++ Header
**Hash:** `8673e99bd10d2457277e3c16df54b18294b816088c090e9bc81244ed8f01a6b0`

## Role

Relay duty cycle tracking and compressor pattern generation.

## Verified API / Symbols

- Classes: RelayDutyWindow, CompressorPattern

- Functions: recordSecond, dutyPercent, sampleCount, recordMinute, complete, startReplay, nextReplayMinute, replayPosition

## Key Dependencies

- Arduino.h


## Side Effects

- No significant side effects

## Tests

- No dedicated automated test identified

## Change Risks

- **HIGH**: Relay control affects hardware

---
*Last modified (hash): 8673e99bd10d2457277e3c16df54b18294b816088c090e9bc81244ed8f01a6b0*