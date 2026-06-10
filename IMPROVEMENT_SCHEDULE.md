# SGNode Improvement Backlog

**Updated:** 2026-06-10

**Current release:** alpha 0.5.0

This backlog tracks the active alpha work. Historical release contents live in the release notes; this file should stay focused on the next useful checks and implementation steps.

## Current Alpha Status

- Float/Base completed a first real fermentation run.
- SGNode Plug firmware and Base integration exist, but Plug control has not yet passed the water/fridge validation run.
- New batches are gated by quick zero calibration and a required `Put Float In Brew` confirmation before CSV logging starts.
- The current Float reference build removed the BMP180/GY-68. BMI160 chip temperature is kept as a diagnostic value only.
- Reference Float deep sleep current is about `0.95 mA` after practical low-power changes.
- Low-level Plug test sketches are kept local-only and ignored by Git.

## Completed Since v0.4.0

- Batch-start quick zero calibration via Float ACK command.
- Required `Put Float In Brew` action to separate setup packets from real batch logging.
- Manage Brew delete geometry and old-batch directory deletion fixes.
- SGNode Plug alpha firmware for Gosund SP1 / ESP8285.
- Base/Plug ESP-NOW command and status protocol.
- Base-side Plug MAC learning, persisted status, command sender, UI diagnostics, and CSV fields.
- Brew Wizard `SGNode Plug` step for per-batch Plug Auto Mode.
- Hardware documentation update for BMP removal, CP2302 pin isolation, and realistic regulator-swap benefit.

## Active Alpha Priorities

### 1. SGNode Plug Water/Fridge Validation

- Run the Plug with water, both DS18B20 sensors, and the fridge before using Plug Auto Mode on beer.
- Confirm relay polarity, command acknowledgement, stale-status handling, and compressor duty logging.
- Validate PI outer-loop behavior for realistic batch volumes and temperature ramps.
- Check cold-crash behavior with the 1.0 C absolute air-target clamp, static 3 K clamp, and dynamic 5 K clamp.
- Tune default `Tn` values after measured water-test response data exists.

### 2. Batch Start And Logging Validation

- Re-test the full new-batch flow on hardware: wizard, quick zero, `Put Float In Brew`, first logged CSV row.
- Confirm packets received before `Put Float In Brew` update the live display but do not enter the batch CSV.
- Confirm completed batches remain read-only enough for normal UI use.
- Keep active-batch restore behavior covered when older and newer batch directories coexist.

### 3. Fermentation Logic Validation

- Compare dry-hop, cold-hop, cold-crash, and package recommendations against real fermentation data.
- Validate action hysteresis so repeated recommendations do not appear and disappear around SG thresholds.
- Continue tuning ETA from smoothed recent SG slope and phase state.
- Store enough yeast performance summary data for future learning, but defer broad learning features until basic thresholds are stable.

### 4. Float Hardware And Power Validation

- Repeat sleep-current measurement after the BMP removal and calibration ACK changes.
- Confirm the BMI160-only build wakes, sends, receives calibration commands, and returns to deep sleep reliably.
- Document any remaining board-specific leakage paths only after measurement.
- Do not reintroduce BMP/BMP180 libraries or wiring for the current reference Float.

### 5. Documentation And Release Hygiene

- Keep README, hardware notes, wiring diagram, release notes, and Plug plan aligned before each GitHub push.
- Keep historical release notes historical; new behavior belongs in the current release notes.
- Keep local-only hardware test sketches out of Git while documenting their purpose.
- Revisit beta status only after Plug water/fridge validation is complete.

## Beta Gate

SGNode should stay alpha until these points are true:

- Plug water/fridge test passes without unsafe relay behavior.
- Plug and Base logs show coherent Float plus Plug rows over a realistic run.
- Batch-start gating is stable on real hardware.
- No critical SD/history loss is observed during normal reboot and batch-management flows.
- The documentation describes the actual hardware and required libraries without stale BMP or test-sketch assumptions.

## Later Ideas

- Sugar/priming calculator based on batch volume, target CO2, beer temperature, and bottle size.
- Centralized error code model.
- More robust settings screen.
- Export helpers for SD card data.
- Automated simulator tests for calculations and state machine.
- Optional additional calibration points if real Float data proves they are needed.

## Rule For Alpha

Prefer small, testable fixes over broad refactors. Every change should preserve existing manual mode, Auto Mode, SD history, batch-start gating, and calibration behavior.
