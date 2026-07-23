# SGNode Architectural Decisions

## Protocol Design

### ESP-NOW Payload Versioning
**Decision:** Protocol version 2 with CRC16-CCITT checksums.

**Rationale:** Ensures compatibility across Base/Float/Plug and detects transmission errors.

**Impact:** Changing payload structure requires version bump and coordinated firmware updates.

### Polynomial Calibration in Float EEPROM
**Decision:** Store cubic polynomial coefficients in float's EEPROM for SG calibration.

**Rationale:** Float owns calibration; coefficients persist across reboots and don't need base intervention.

**Impact:** Changing calibration algorithm requires EEPROM format migration strategy.

## Temperature Control

### PI Controller with D-Brake
**Decision:** Implement PI control with derivative brake (D-brake) in Plug.

**Rationale:** Prevents compressor overshoot and short-cycling while maintaining target stability.

**Impact:** PI parameters (Kp, Tn, D-brake) are critical for hardware safety.

### Beer Probe vs Air Probe Control
**Decision:** Use beer probe as primary feedback, calculate air target from PI output.

**Rationale:** Direct beer temperature control yields better fermentation profiles.

**Impact:** Air-only mode (post-package) is fallback when beer probe unavailable.

## Fermentation State Machine

### Phase Transitions
**Decision:** Driven by biological gates (SG trend, attenuation) and temperature stability, not elapsed time alone.

**Rationale:** Fermentation biology varies; automated transitions adapt to actual process.

**Impact:** User may need to manually advance phases if data is incomplete.

### Auto-Start Policies
**Decision:** D-rest and cold crash auto-start only when biological gates AND Plug status gates satisfied.

**Rationale:** Temperature control must be reliable before automatic phase changes.

**Impact:** Plug communication failures block automatic progression.

## Data Persistence

### SD Card Format
**Decision:** FAT32, JSON for metadata (profile.json, target.json), CSV for logs.

**Rationale:** Simple, portable, readable, works with standard tools.

**Impact:** Changing format requires migration strategy or backward compatibility.

### Batch Lifecycle
**Decision:** Batch starts only after "Put Float In Brew" confirmation.

**Rationale:** Prevents handling/air measurements from contaminating batch history.

**Impact:** Float packets before confirmation update live display but not CSV log.

## UI Design

### No External Connectivity
**Decision:** No web UI, phone app, or cloud. All functionality on touchscreen base.

**Rationale:** Self-contained, offline, no network dependencies, privacy-friendly.

**Impact:** No remote access, no data sync, no mobile convenience.

### Theme System
**Decision:** Light/dark theme with persistent setting.

**Rationale:** User preference and usage conditions (low light).

**Impact:** Theme state stored in EEPROM/SD, not memory-only.

## Power Management

### Float Deep Sleep
**Decision:** Deep sleep between measurements (configurable interval 60-600s).

**Rationale:** Battery-powered float must maximize sleep time.

**Impact:** Measurement interval affects data resolution and battery life.

### Zero Calibration
**Decision:** Two separate systems: polynomial SG calibration (manual) and zero-cal (automatic via ACK).

**Rationale:** Different purposes: SG from known solutions vs tilt orientation.

**Impact:** Zero-cal happens automatically during batch start; polynomial requires user interaction.

## Build System

### Arduino IDE Only
**Decision:** No CLI build system. Use Arduino IDE for compilation and upload.

**Rationale:** ESP32/ESP8266 ecosystem standard, simpler for hobbyist users.

**Impact:** CI/CD automation requires special setup or manual process.

## Debug Strategy

### Debug Levels
**Decision:** Four levels: QUIET, ERROR, INFO, VERBOSE.

**Rationale:** Balance between diagnostics and production noise.

**Impact:** Set via preprocessor define; not runtime-configurable.
