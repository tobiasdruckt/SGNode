# SGNode Serial UI Automation Test Plan

**Scope:** debug-only UI and firmware regression testing through `SGNODE_UI_TEST_HARNESS`.

**Current release:** alpha 0.5.0

The harness sends serial commands into the native touchscreen UI. Touch simulation enters the same event path as real touch input. The test suite must never commit a destructive user action unless the same final confirmation path a user would press has completed.

## Running

Serial settings:

```text
115200 baud
Newline line ending
```

Basic checks:

```text
help
screen
dump_ui
selftest
run_input_validation_tests
run_sensor_edge_tests
run_state_logic_tests
run_ui_safety_tests
run_batch_restore_tests
run_fallback_log_tests
run_completed_batch_tests
run_manage_brew_ui_tests
run_ui_regression_tests
```

Prefer the smaller focused commands during development because they keep ESP32 loop stack usage low. The full regression command prints one result per test:

```text
OK TEST CalibrationAbortSafety PASS
ERR TEST CancelConfirmation FAIL step=discard_dialog expected=visible actual=missing
```

Then it prints one summary:

```text
ERR ui_regression total=36 passed=28 failed=8 first_failure=InvalidBrixInput:validation
```

## Test Matrix

| # | Test | Method | Expected |
| ---: | --- | --- | --- |
| 1 | Calibration Abort Safety | Soft calibration draft, no save | Existing coefficients unchanged |
| 2 | Calibration Power-Loss Simulation | Partial draft reset | Previous calibration still produces valid SG |
| 3 | Calibration Final Save | Save/reload temporary coefficients, then restore old values | New coefficients reload, old values restored |
| 4 | New Batch Abort Before Confirm | Open wizard, enter partial data, back out | No `profile.json` |
| 5 | New Batch Abort After Partial Draft | Forward/back/cancel flow | No committed batch |
| 6 | New Batch Final Confirm | Create `test_batch` | `profile.json`, `target.json`, and initial `log.csv` exist |
| 7 | Batch Start Gate | Complete wizard, then simulate pre-start packets | Live display may update, CSV rows wait for `Put Float In Brew` |
| 8 | Quick Zero Calibration Gate | New batch starts with level-float action | Calibration ACK path runs before brew-start confirmation |
| 9 | Plug Wizard Toggle | Open Brew Wizard Plug step | `plugControlEnabled` stores selected per-batch mode |
| 10 | Duplicate Batch Name | Existing test batch then new wizard | New batch id is unique |
| 11 | Invalid Brix Input | Calculation/validation probe | Invalid values rejected or flagged |
| 12 | Invalid Batch Size | Validation probe | Invalid volume rejected |
| 13 | Invalid Attenuation | FG bounds probe | No invalid FG |
| 14 | Auto Mode Toggle Consistency | Toggle source fields | Correct `attenuationSource` |
| 15 | Yeast Preset Missing/Corrupt | Missing preset lookup | Fallback preset exists |
| 16 | OG Verification Abort | Pending choice without selection | `effectiveOG` unchanged |
| 17 | OG Verification Recipe | Confirm recipe OG | `effectiveOG == recipeOG` |
| 18 | OG Verification Measured | Confirm measured OG | `effectiveOG == measuredOG` |
| 19 | Sensor Dropout | Mock invalid SG/temp | UI remains responsive, sensor issue visible |
| 20 | Extreme Sensor Values | Mock SG/temp extremes | No NaN/inf/crash |
| 21 | State Machine Spike | Normal drop then SG spike | State does not reset to idle |
| 22 | Stable Gravity Detection | Short/long stability simulation | Not ready early, stable later |
| 23 | Diacetyl Rest Logic | Lager and ale profiles | Lager triggers, ale does not auto-trigger |
| 24 | Reboot During Active Batch | Soft reload from SD | Active profile reloads |
| 24b | Mock Time Isolation | Set mock time, then write via normal CSV logger | CSV epoch uses RTC time, not mock time |
| 24c | Historical Mixed Log Filter | Build log with rows before and after `createdAt` | Loaded/skipped/error counts expose filtered rows |
| 24d | Active Batch Restore Safety | Create older active batch and newer inactive batch | Boot restore must not blindly load highest batch number |
| 24e | Fallback Uses Log Data | No active marker, higher empty batch exists | Select batch with valid post-`createdAt` log data |
| 25 | Manage Brew Delete Geometry | Open Manage Brew with multiple batches | Next/delete touch targets do not overlap |
| 26 | SD Card Missing | Fault-injection placeholder | Must be implemented with SD mock/hardware test |
| 27 | Corrupt profile.json | Fault-injection placeholder | Must be implemented with file mutation/recovery |
| 28 | Corrupt target.json | Fault-injection placeholder | Must be implemented with file mutation/regeneration |
| 29 | Log Append Failure | Fault-injection placeholder | Must be implemented with SD write failure mock |
| 30 | Touch Spam | Rapid next/back taps | No crash/double create |
| 31 | Back Navigation | Enter, forward, back | Values retained |
| 32 | Cancel Confirmation | Press cancel after edits | `Discard changes?` dialog appears |
| 33 | Completed Batch Protection | Completed state lock probe | Completed batch protected from overwrite |
| 34 | Dashboard No Batch | Delete test batch | Dashboard survives no active batch |
| 35 | Long Text Input | Long batch name | Bounded safely |
| 36 | Special Characters | Unsupported chars and safe ASCII | No invalid paths or overflow |
| 37 | Memory/Heap Stability | Repeated create/delete | Heap does not steadily fall |
| 38 | Full End-to-End Auto Mode | Auto batch plus mock SG | ETA/recommendations present |
| 39 | Full End-to-End Manual Mode | Manual profile | `attenuationSource == manual` |

## Remaining Alpha Gaps

Some checks still require fault injection, hardware, or longer runs before they can be considered release gates:

- SD missing fault injection
- Corrupt profile recovery
- Corrupt target regeneration checks
- Log append failure injection
- Strong numeric validation for Brix, batch size, and attenuation
- Plug water/fridge validation with real sensors and relay hardware
- Long-duration batch-start and completed-batch regression runs on the device

These gaps are useful: they define alpha hardening work before a beta label.

## Production Rule

Before production flashing:

```cpp
#define SGNODE_UI_TEST_HARNESS 0
```

The test harness must not ship enabled in a normal build.
