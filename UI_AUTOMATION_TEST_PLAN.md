# SGNode Serial UI Automation Test Plan

**Scope:** debug-only UI and firmware regression testing through `SGNODE_UI_TEST_HARNESS`.

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
| 6 | New Batch Final Confirm | Create `test_batch` | `profile.json`, `target.json`, `log.csv` exist |
| 7 | Duplicate Batch Name | Existing test batch then new wizard | New batch id is unique |
| 8 | Invalid Brix Input | Calculation/validation probe | Invalid values rejected or flagged |
| 9 | Invalid Batch Size | Validation probe | Invalid volume rejected |
| 10 | Invalid Attenuation | FG bounds probe | No invalid FG |
| 11 | Auto Mode Toggle Consistency | Toggle source fields | Correct `attenuationSource` |
| 12 | Yeast Preset Missing/Corrupt | Missing preset lookup | Fallback preset exists |
| 13 | OG Verification Abort | Pending choice without selection | `effectiveOG` unchanged |
| 14 | OG Verification Recipe | Confirm recipe OG | `effectiveOG == recipeOG` |
| 15 | OG Verification Measured | Confirm measured OG | `effectiveOG == measuredOG` |
| 16 | Sensor Dropout | Mock invalid SG/temp | UI remains responsive, sensor issue visible |
| 17 | Extreme Sensor Values | Mock SG/temp extremes | No NaN/inf/crash |
| 18 | State Machine Spike | Normal drop then SG spike | State does not reset to idle |
| 19 | Stable Gravity Detection | Short/long stability simulation | Not ready early, stable later |
| 20 | Diacetyl Rest Logic | Lager and ale profiles | Lager triggers, ale does not auto-trigger |
| 21 | Reboot During Active Batch | Soft reload from SD | Active profile reloads |
| 21b | Mock Time Isolation | Set mock time, then write via normal CSV logger | CSV epoch uses RTC time, not mock time |
| 21c | Historical Mixed Log Filter | Build log with rows before and after `createdAt` | Loaded/skipped/error counts expose filtered rows |
| 21d | Active Batch Restore Safety | Create older active batch and newer inactive batch | Boot restore must not blindly load highest batch number |
| 21e | Fallback Uses Log Data | No active marker, higher empty batch exists | Select batch with valid post-`createdAt` log data |
| 22 | SD Card Missing | Fault-injection placeholder | Must be implemented with SD mock/hardware test |
| 23 | Corrupt profile.json | Fault-injection placeholder | Must be implemented with file mutation/recovery |
| 24 | Corrupt target.json | Fault-injection placeholder | Must be implemented with file mutation/regeneration |
| 25 | Log Append Failure | Fault-injection placeholder | Must be implemented with SD write failure mock |
| 26 | Touch Spam | Rapid next/back taps | No crash/double create |
| 27 | Back Navigation | Enter, forward, back | Values retained |
| 28 | Cancel Confirmation | Press cancel after edits | `Discard changes?` dialog appears |
| 29 | Completed Batch Protection | Completed state lock probe | Completed batch protected from overwrite |
| 30 | Dashboard No Batch | Delete test batch | Dashboard survives no active batch |
| 31 | Long Text Input | Long batch name | Bounded safely |
| 32 | Special Characters | Unsupported chars and safe ASCII | No invalid paths or overflow |
| 33 | Memory/Heap Stability | Repeated create/delete | Heap does not steadily fall |
| 34 | Full End-to-End Auto Mode | Auto batch plus mock SG | ETA/recommendations present |
| 35 | Full End-to-End Manual Mode | Manual profile | `attenuationSource == manual` |

## Known Intentional Failures

Some tests are expected to fail until the firmware gains the corresponding safety feature:

- Cancel confirmation dialog
- SD missing fault injection
- Corrupt profile recovery
- Corrupt target regeneration checks
- Log append failure injection
- Completed batch write protection
- Strong numeric validation for Brix, batch size, and attenuation
- Active-batch restore metadata (`active_batch.txt` or equivalent)

These failures are useful: they define alpha hardening work before a non-debug build.

## Production Rule

Before production flashing:

```cpp
#define SGNODE_UI_TEST_HARNESS 0
```

The test harness must not ship enabled in a normal build.
