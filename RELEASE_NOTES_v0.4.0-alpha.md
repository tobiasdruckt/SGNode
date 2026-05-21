# SGNode alpha 0.4.0 Release Notes

**Date:** 2026-05-21  
**Status:** Alpha test build  
**Focus:** Standalone intelligent fermentation assistant on the ESP32 base station

## Summary

alpha 0.4.0 moves SGNode beyond passive monitoring. The base station now provides a native touchscreen Brew Wizard, yeast Auto Mode, batch management, richer charts, local recommendations, ETA prediction, and improved SD-backed persistence.

The system remains fully standalone. There is no web UI, phone app, REST frontend, or externally served dashboard.

## Added

- Brew Wizard for batch setup on the 4-inch touchscreen
- Auto Mode with yeast behavior presets
- Custom yeast creation, editing, deletion, and SD storage
- Batch management: continue, copy, and delete
- Brix to SG conversion and recipe OG storage
- Measured OG verification and effective OG selection
- Expected FG, attenuation, ABV, gravity slope, and fermentation rate calculations
- Target curve generation for gravity and attenuation
- Fermentation state machine
- Recommendation engine and ETA predictor
- Dashboard panels for batch, phase, yeast, ETA, ABV, FG, and action text
- Target vs actual charts for SG, temperature, angle, and ABV
- Persistent light/dark theme
- More compact Details view diagnostics
- Serial debug levels to reduce normal logging noise

## Changed

- Boot screen now shows `alpha 0.4.0`.
- Base station startup serial text identifies alpha 0.4.0 when `DEBUG_INFO` or higher is enabled.
- CSV logs are stored per batch under `/data/batches/<batch_id>/log.csv`.
- Historical CSV data is finalized into the UI immediately after boot/load.
- Graph and Battery 24h loading no longer drops every second row on small files.
- New Brew avoids reusing existing batch directories or old `log.csv` files.
- CSV loading filters rows older than the active batch creation time when timestamps are valid.
- Details System section now shows separate SD, CSV loaded/total, and buffer counters.

## Fixed

- Historical graphs requiring a new float packet before becoming available.
- New batch paths accidentally reusing old log files in some edge cases.
- Battery view 24h mode showing fewer loaded rows than expected.
- Details diagnostic text being clipped.
- Excessive serial output from float packet receive, CSV loading, SD writes, battery regression, UI taps, and calibration chatter.

## Known Risks

- Long-duration fermentation runs are still needed to validate phase thresholds and ETA behavior.
- SD buffering can still lose unsaved RAM entries on sudden reset or power loss.
- Some UI screens are dense for a 4-inch display and need continued real-device testing.
- Float battery drain requires measurement on hardware; calibration mode and sensor sleep behavior are the first suspects.
- Optional additional calibration points require float firmware changes.

## Recommended Test Pass

1. Create a new batch and confirm the new directory, `profile.json`, `target.json`, and `log.csv`.
2. Reboot before receiving new data and confirm historical rows appear immediately.
3. Test Battery 6h, 24h, and All with a small CSV and confirm loaded/total counts.
4. Create a custom yeast, reboot, and confirm it appears in New Brew.
5. Continue, copy, and delete batches from Manage Brew.
6. Confirm light/dark theme persists after reboot.
7. Run a multi-day fermentation to evaluate ETA, state transitions, and SD flush behavior.
