# SGNode Temperature Profiles For Yeast And Beer Styles

## Summary

- Temperature guidance is built as a profile-family system for all existing yeast presets, not as a W-34/70/Pils-only special case.
- Primary derivation: yeast and `curveTemplate` first; beer style only acts as a light modifier.
- Biological temperature intent and Plug control tuning stay separate:
  - Base owns the beer target profile and phase/action state.
  - Plug owns local fridge control and turns the requested beer target into an air target.
- Profile data is stored in `profile.json`. Existing batches without `temperatureProfile` remain legacy and continue using the current min/max midpoint logic.
- The wizard stays compact: SGNode generates an automatic profile, shows a summary, and keeps Advanced editing to core values.
- First Plug controller defaults now come from the 13 L water/fridge validation run, including clean 9 C and 4 C holds. Yeast/style temperature-profile defaults still need to be derived separately from fermentation intent and later real-batch data.
- After manual package confirmation, SGNode can continue supporting bottle conditioning and storage with Plug air-only temperature control.

## Profile Families

Supported v1 families:

| Family | Presets / Use |
| --- | --- |
| `clean_ale` | US-05, Nottingham, Generic Ale |
| `fast_ale` | S-04 |
| `slow_clean_ale` | K-97 / Koelsch / German Ale |
| `lager` | W-34/70, S-23, Generic Lager |
| `very_fast_ale` | Voss Kveik |
| `high_attenuation_saison` | Belle Saison |

Custom yeasts derive their profile from `curveTemplate`, recommended min/max temperature, fermentation speed, and diacetyl-rest flag.

Main target selection:

- `lager`: lower yeast range. W-34/70 default: pitch 9 C, main 10 C, D-rest 15 C, crash 4 C, lagering hint 2 C.
- `clean_ale`: middle to slightly lower yeast range.
- `fast_ale`: middle yeast range with shorter hold times.
- `slow_clean_ale`: cool, clean ale range; D-rest when the preset recommends it.
- `very_fast_ale`: warm range from the yeast preset; no automatic D-rest by default.
- `high_attenuation_saison`: warm, gently rising profile toward the upper yeast range; no classic D-rest by default.

Style modifiers stay intentionally light:

- Hoppy styles enable dry-hop/cold-hop action logic and block cold crash until hop handling is resolved.
- Wheat, stout, and porter may shift main target slightly, but never outside yeast limits.
- Cider, mead, and other styles use Generic + Edit.

## Data Model

`profile.json` receives a versioned `temperatureProfile` object.

Core fields:

- `version`
- `family`
- `presetId`
- `source`
- `phases`
- `advanced`
- `status`

Phase fields:

- `id`
- `label`
- `targetC`
- `holdHoursAfterReached`
- `triggerType`
- `rampClass`
- `requiresActionResolved`
- `autoStartPolicy`

Status fields:

- `activePhaseId`
- `targetReached`
- `targetReachedAt`
- `phaseStartedAt`
- `autoStarted`

Advanced fields:

- `pitchC`
- `mainC`
- `dRestC`
- `crashC`
- `lagerC`
- `mainHoldHours`
- `dRestHoldHours`
- `crashHoldHours`
- `targetCO2`
- `carbonationC`
- `carbonationDays`
- `storageC`
- `storageDaysHint`

Legacy behavior:

- Batches without `temperatureProfile` are not migrated automatically.
- The current min/max midpoint target remains the fallback.
- CSV history and old `profile.json` files must remain readable.

## Wizard

Default compact flow:

1. Select beer style.
2. Select yeast preset or custom yeast.
3. Wizard generates the temperature profile automatically.
4. Wizard shows a compact summary: pitch, main, D-rest if present, crash, lager/package hint.
5. If a paired Plug is available, offer Plug Auto Mode.
6. Advanced allows editing the core temperatures, hold times, and target CO2.

Wizard rules:

- Yeast profile is authoritative.
- Beer style may only apply small modifiers within yeast limits.
- Plug Auto Mode selects Plug beer probe as the authoritative temperature for Base display, logging, and analysis.
- If Plug is not available, the temperature profile remains advisory.

## Hybrid Phase Logic

Temperature phases are not advanced by elapsed recipe time alone.

Biological gates:

- SG trend
- fermentation phase
- attenuation / FG stability
- batch actions such as dry hop, cold hop, D-rest, cold crash, package

Temperature stability gates:

- Plug beer sensor reaches target.
- Target remains within tolerance for the phase hold time.
- Plug status is fresh.
- Latest Plug command was acknowledged by the Plug or fresh Plug status confirms the requested target.

Auto-start policy:

- D-rest may auto-start when fermentation status fits, Plug is fresh, and the Plug has acknowledged the latest target command or reports the requested target.
- Cold crash may auto-start when D-rest is resolved, hop actions are resolved, FG is stable, Plug is fresh, and the Plug has acknowledged the latest target command or reports the requested target.
- Package remains manual.
- After manual package confirmation, SGNode may enter a post-package temperature-support mode for carbonation and storage.

Important naming decision:

- A future implementation should distinguish `coldCrashStarted` from `coldCrashDone`.
- The current profile model uses action acknowledgement fields that are too ambiguous for automatic cold-crash control.

## Plug Control Boundary

Base sends:

- requested beer target
- ramp intent / ramp class
- batch size
- controller parameters when explicitly configured
- post-package air-only target when the batch is packaged and the beer probe is no longer available

Plug controls locally:

- beer probe is the controlled process value
- fridge-air probe is the inner two-point control value
- PI / D-brake / anti-windup derives fridge-air target
- relay timing and compressor protection remain local
- in post-package air-only mode, the beer probe is ignored and the fridge-air probe directly controls the relay around the requested air target

Base displays and logs:

- Plug beer temperature as primary temperature when Plug Auto Mode is active
- Float temperature as diagnostic `float_temperature`
- requested beer target
- derived air target
- relay state and 10-minute duty
- controller state and faults
- post-package air-only phase, air target, air actual, and duty even when Float packets are absent

## Post-Package Temperature Support

Package itself remains manual: the user confirms that bottling/kegging is done.

After confirmation, SGNode may continue the same batch in a post-package phase:

- `carbonation`: hold fridge air at the carbonation temperature for the configured duration.
- `storage` / `lagering`: move to the configured storage temperature after carbonation time has elapsed.

Control source changes:

- Beer/vessel probe is expected to be unavailable after packaging.
- Float may be unavailable or out of radio range.
- Plug switches to air-only control using the fridge-air DS18B20.
- Base no longer treats missing Float or missing Plug beer probe as a blocker for post-package temperature support.

Logging changes:

- During fermentation, combined CSV rows can remain Float-triggered.
- During post-package air-only mode, Base must also be able to append Plug-timed rows, ideally every 10 minutes.
- Post-package rows leave SG, angle, Float battery, and Float temperature empty or sentinel-coded, while Plug air temperature, air target, relay state, duty, and faults remain populated.
- CSV readers must distinguish measurement source/state so post-package air-only rows do not corrupt SG/attenuation charts.
- The batch remains active for temperature support until the user ends tracking or the storage period is complete.

UI changes:

- Package screen shows sugar and carbonation guidance before confirmation.
- After confirmation, Dashboard shows `Carbonation` or `Storage` instead of fermentation phase.
- Live/Details show Plug Air as the controlled value and mark Beer/Float as unavailable rather than stale failure.
- Manage Brew must allow ending post-package tracking manually.

## Plug-Test Dependency

The current `plug_test_schedule` is only a validation tool. It will be replaced by the real `temperatureProfile` model after tuning.

Current 13 L water/fridge validation goals:

- Start from a clean stable beginning, not the disturbed warm-water recovery section.
- SG-vs-temperature analysis starts only with the new clean test data.
- 22 -> 12 C: delay, cooling rate, air minimum, duty, undershoot/overshoot.
- 12 -> 4 C: cold-crash behavior, clamp behavior, duty, reachable air target.
- 4 -> 15 C: passive warming, residual cooling, overshoot risk.

Values to keep validating from test data:

- normal ramp default
- cold-crash ramp default
- absolute air target minimum
- whether static/dynamic offset clamps are needed or can be replaced by tuned PI + D-brake behavior
- `Kp`, `Tn`, D-brake defaults across other fill levels
- batch-size-to-`Tn` scaling
- beer-temperature smoothing window

Do not derive permanent defaults from:

- manually disturbed warm-water mixing
- open-fridge periods
- missing Plug sensors during bench flashing
- Float/BMI chip temperature

## Packaging Guidance

Packaging guidance is generated from style and Advanced settings.

Package screen should show:

- sugar grams per 0.5 L bottle
- total sugar amount
- target CO2
- carbonation temperature
- carbonation duration hint
- lagering/storage hint where useful

v1 assumptions:

- household sugar / sucrose
- manual package confirmation
- active post-package temperature support is allowed when Plug is enabled
- 0.5 L bottles plus total batch quantity are the primary output

## Test Plan

Profile generation:

- Every built-in yeast preset maps to the expected profile family.
- Custom yeast uses `curveTemplate`, min/max temperature, fermentation speed, and D-rest flag.
- Cider, mead, and other styles use Generic + Edit.

Wizard:

- Compact view shows the generated profile correctly.
- Advanced stores core values in `profile.json`.
- Legacy batches without `temperatureProfile` remain unchanged.

Control integration:

- Base sends the correct beer target for the active phase.
- Plug beer temperature replaces Float temperature for Plug Auto batches in Live, Dashboard, Graph, and new CSV rows.
- Float temperature remains logged as `float_temperature`.
- Post-package Plug air-only rows are logged every 10 minutes even when Float packets are absent.
- Plug test schedule does not leak into normal batch profile behavior.

Actions:

- Dry hop blocks cold crash until done, removed, or skipped.
- D-rest and cold crash start only when all biological and Plug gates are satisfied.
- Package shows sugar and carbonation recommendation.
- After Package confirmation, Plug air-only mode supports carbonation and storage temperatures.

Regression:

- Arduino compile Base and Plug.
- Batch start gating stays stable.
- Manage Brew stays usable.
- Existing CSV history remains readable.
- SG/attenuation charts ignore post-package air-only rows.
- Old batches without temperature profiles keep legacy behavior.

## Assumptions

- Yeast profile is the main source; style is a modifier.
- All existing presets receive v1 temperature profiles.
- Old batches are not auto-migrated.
- Initial Plug controller defaults are based on the first 13 L water/fridge validation. Broader ramp/profile defaults still need more validation across fill levels and real fermentation heat.
- Packaging starts as guidance, then may continue as Plug-supported carbonation/storage temperature management after manual confirmation.
- Priming output focuses on 0.5 L bottles plus total amount.
