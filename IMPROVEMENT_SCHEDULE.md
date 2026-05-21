# SGNode Improvement Backlog

**Updated:** 2026-05-21  
**Current release:** alpha 0.4.0

This file replaces the old v0.3.0 four-week schedule. The previous plan contained useful ideas, but it duplicated release notes and no longer matched the current Phase 1 assistant firmware.

## Completed Since v0.3.0

- Base station debug levels and quieter serial output
- Native touchscreen Brew Wizard
- Yeast Auto Mode and custom yeast management
- Batch profile, target, and log storage under `/data/batches/`
- Dashboard, Details, Battery, and target-vs-actual chart improvements
- Historical CSV reload after reboot
- Theme persistence
- Batch management and safer new-batch log isolation

## Active Alpha Priorities

### 1. SD Logging Reliability
- Flush buffered readings before reboot where possible.
- Make buffer state visible enough for field testing.
- Consider shorter flush interval or explicit flush on view transitions.
- Add protection against power loss during profile/log writes.

### 2. Float Battery Drain
- Measure actual sleep current.
- Confirm deep sleep is reached after every send.
- Check whether BMP180/BMI160 can be suspended or powered down between reads.
- Review wake interval and calibration-mode exit behavior.

### 3. Fermentation Logic Validation
- Test state transitions over real fermentations.
- Tune lag, active, diacetyl, stable FG, and packaging thresholds.
- Compare ETA predictions against real completion times.
- Store enough yeast performance summary data for future learning.

### 4. UI Consistency
- Continue real-device checks for clipped text and touch targets.
- Keep Details diagnostic, Dashboard, and graph screens readable in both themes.
- Avoid adding dense text where a compact value/list is clearer.

### 5. Calibration Extension
- Decide whether optional calibration points are required for alpha or later.
- If yes, update float protocol, float EEPROM format, base UI, and coefficient fitting together.

## Later Ideas

- Centralized error code model
- More robust settings screen
- Export helpers for SD card data
- Automated simulator tests for calculations and state machine
- Hardware power profiling notes per board revision

## Rule For Alpha

Prefer small, testable fixes over broad refactors. The project is now complex enough that every change should preserve existing manual mode, Auto Mode, SD history, and calibration behavior.
