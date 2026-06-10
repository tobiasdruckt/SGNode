# SGNode alpha 0.5.0 Release Notes

**Date:** 2026-06-10  
**Status:** Alpha test build  
**Focus:** Batch-start safety, first real-batch learnings, and SGNode Plug alpha groundwork

## Summary

alpha 0.5.0 keeps SGNode in alpha. Float/Base have completed a first real fermentation run, but SGNode Plug temperature control still needs water/fridge validation before a beta label is justified.

## Added

- Batch-start zero calibration using the normal Float ACK/downlink path.
- Required `Put Float In Brew` action before new-batch logging and fermentation analysis start.
- SGNode Plug alpha firmware for Gosund SP1 / ESP8285.
- Shared Base/Plug ESP-NOW command and status protocol.
- Base-side Plug MAC learning, status handling, command sender, UI diagnostics, and CSV fields.
- Brew Wizard Plug Auto Mode toggle.

## Changed

- Base boot screen and serial startup text now show `alpha 0.5.0`.
- New-batch `createdAt`, `log.csv`, and target curves start from `Put Float In Brew` confirmation.
- Float packets received before `Put Float In Brew` update Live view but are not written to the active CSV.
- Float temperature now uses BMI160 chip temperature because the BMP180/GY-68 was removed from the reference hardware.
- Manage Brew delete uses the batch directory name and removes the whole batch directory.
- Plug test sketches are local-only and ignored by Git.

## Fixed

- Manage Brew delete and next buttons overlapping on the 4-inch display.
- Zero-calibration debug ACK messages sticking in the Dashboard after success.
- Quick zero calibration rejecting the real mounted sensor because of a too-narrow raw acceleration magnitude window.

## Hardware Notes

- BMP180/GY-68 removed from the Float; BMI160 chip temperature is diagnostic only.
- USB-UART leakage was reduced by isolating pin 8 on the tested CP2302/USB-UART chip.
- ME6220 regulator swap was tested but did not provide enough benefit to recommend as a priority modification.
- Reference Float deep sleep current after practical changes is about `0.95 mA`.

## Known Risks

- SGNode Plug remains alpha until water/fridge tests validate controller stability, logging, compressor timing, and fallback behavior.
- Float BMI160 temperature is not representative beer temperature.
- Long-duration runs are still needed to tune phase thresholds, ETA behavior, and Plug PI parameters.

## Recommended Test Pass

1. Create a new batch and verify `Level Float` then `Put Float In Brew`.
2. Confirm packets before `Put Float In Brew` are live-only and not appended to `log.csv`.
3. Confirm `log.csv` starts at the real batch confirmation time.
4. Test Manage Brew continue, copy, and delete with several batches.
5. Run SGNode Plug with water in the fridge before enabling it on a real fermentation.
