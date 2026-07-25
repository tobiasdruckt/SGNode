# SGNode_Base/src/domain/recommendation_engine.cpp

**Type:** C++ Source
**Hash:** `d3c7b44387fc126476ee545b7ae7c8e799146917b08d747409dab619912bea5f`

## Overview / Role

Core functionality module

## Verified API & Symbols

### Functions

- `void sensorIssue (const char* detail) const`

- `void build (FermentationPhase phase, bool ogVerified, bool ogWarning, float attenuation, float expectedFG, float currentSG)`

- `void build (const BrewProfile& profile, FermentationPhase phase, bool ogVerified, bool ogWarning, float attenuation, float expectedFG, float currentSG, float temperatureC, float gravityDeltaPerHour, unsigned long elapsedSeconds) const`

## Key Dependencies

**Local:**
- `SGNode_Base/src/domain/recommendation_engine.h`

**System/External:**
- `<stdio.h>`
- `<string.h>`

## Side Effects & Hardware Access

- Sensor reading
- Temperature target calculation

## Change Risks

- **MEDIUM**: Domain logic affects business rules

---
*Last modified (hash): d3c7b44387fc126476ee545b7ae7c8e799146917b08d747409dab619912bea5f*