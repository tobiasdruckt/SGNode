# SGNode Plug Implementation Prompts

Working notes for adding an ESP8285-based smart plug as a standalone fridge controller for SGNode.

These notes are intentionally split into small implementation prompts. Each prompt should be usable as a focused task later.

## Current Implementation Status

SGNode Plug is implemented and has passed initial bench plus first 13 L water/fridge validation, including clean 9 C and 4 C holds. It remains **alpha** until other fill levels and at least one real fermentation validate the controller.

| Stage | Status | Notes |
| --- | --- | --- |
| 1. Hardware validation | Done for current bench hardware | Gosund SP1, ESP8285, power metering removed, GPIO4/GPIO5 freed, relay active-high on GPIO14 |
| 2. Dual DS18B20 serial test | Done for current hardware | `SGNode_Plug_Sensor_Relay_Test` is kept local-only and ignored by Git; sensor assignment was validated before Plug firmware testing |
| 3. Plug firmware skeleton | Implemented | `SGNode_Plug/SGNode_Plug.ino` and supporting modules exist |
| 4. Local air controller | Implemented | Asymmetric air thresholds implemented; current defaults are off at air target + 0.5 K and on at air target + 1.1 K |
| 5. Rolling statistics and fallback | Implemented | 10-minute duty and six-hour pattern fallback implemented; replay behavior needs live validation |
| 6. ESP-NOW protocol | Implemented | Shared command/status structs, Base MAC learning, persisted Plug MAC, and status handling are present |
| 7. Outer PI beer controller | Implemented | Explicit Base-sent `Kp`, `Tn`, D-brake, clamps, and air-target derivation are implemented |
| 8. Base batch/UI integration | Implemented | Brew Wizard Plug toggle, Base command sender, status display, and Plug temperature preference are present |
| 9. Shared CSV integration | Implemented | Batch CSV includes Plug status fields while remaining backward-compatible |
| 10. Water tests and tuning | In progress | First 13 L test produced usable defaults; other fill levels and real fermentation validation remain before beta |

Open validation before beta:

- Validate both DS18B20 probes over longer operation on real wiring.
- Run low-voltage relay/contact test without 230 V load.
- Run water/fridge test with several fill levels.
- Validate PI `Kp`, batch-size-to-`Tn`, 3 K hold clamp, 5 K transition clamp, 1.0 C minimum air target, compressor timing, and fallback behavior.
- Confirm Base logs coherent Float + Plug rows during the test.
- Confirm cold-crash transition behavior with water before using Plug auto mode on a live batch.

## Implementation Order

Implement and validate SGNode Plug in the following order. Do not proceed to the next stage until the current stage has passed its checks.

### Stage 1: Hardware Validation

- Confirm GPIO4 and GPIO5 are electrically isolated after removing the power-metering hardware.
- Confirm GPIO14 relay is active-high.
- Confirm relay remains off during boot/reset/flash.
- Document final wiring and sensor cable routing.

Exit criteria:

- GPIO4/GPIO5 can be safely driven/read at 3.3 V.
- Relay behavior is known and repeatable.
- No mains-connected testing is required for unresolved low-voltage wiring.

### Stage 2: Dual DS18B20 Serial Test Sketch

- Create or keep a standalone local hardware-test sketch before implementing SGNode Plug firmware.
- Suggested local path: `SGNode_Plug_Sensor_Relay_Test/SGNode_Plug_Sensor_Relay_Test.ino`.
- Connect fridge-air DS18B20 to GPIO4.
- Connect vessel/beer DS18B20 to GPIO5.
- Use one 4.7 kOhm pull-up to 3.3 V on each data line.
- Read both sensors independently and print both temperatures over Serial.
- Print explicit errors for missing, disconnected, invalid, or CRC-failing sensors.
- Force GPIO14 low immediately at startup.
- After both sensors provide valid values, control the relay using a simple differential test rule:
  - If vessel/beer sensor is warmer than fridge-air sensor, switch cooling relay on.
  - If fridge-air sensor is warmer than vessel/beer sensor, switch cooling relay off.
- Add a small differential hysteresis to prevent relay chatter when both values are nearly equal.
- If either sensor is invalid, keep the relay off.
- Run this hardware test without 230 V AC connected.
- Verify relay contact continuity only with a multimeter.
- Do not connect a refrigerator or other mains load during this test.

Suggested Serial output:

```text
AIR 20.31 C | BEER 20.44 C | AIR_OK=1 | BEER_OK=1 | RELAY=ON | STATE=BEER_WARMER
```

Exit criteria:

- Both sensors are detected consistently across multiple resets.
- Both temperatures update independently.
- Disconnecting either sensor reports the correct individual fault.
- Relay switches on when the beer sensor is sufficiently warmer than the air sensor.
- Relay switches off when the air sensor is sufficiently warmer than the beer sensor.
- Relay does not chatter when both values are nearly equal.
- Invalid sensor data always switches or keeps the relay off.
- Multimeter continuity confirms the expected relay contact state.

### Stage 3: Plug Firmware Skeleton

- Create `SGNode_Plug/SGNode_Plug.ino`.
- Initialize relay safe-off, both sensors, timing, diagnostics, and fixed Base peer MAC.
- Implement controller state machine without switching a real compressor yet.
- Print calculated states and relay requests over Serial.

Exit criteria:

- Firmware boots reliably.
- Sensor readings and controller states are stable.
- No unexpected relay activation occurs.

### Stage 4: Local Air Two-Point Controller

- Implement the inner fridge-air controller.
- Add asymmetric thresholds relative to air target.
- Current defaults: relay off at air target + 0.5 K, relay on at air target + 1.1 K.
- Add minimum compressor off/on timing.
- Test first with a lamp or other non-compressor load if practical.

Exit criteria:

- Relay follows air target and hysteresis correctly.
- Minimum compressor timing cannot be bypassed.
- Relay remains off during invalid startup state.

### Stage 5: Rolling Statistics And Time-Pattern Fallback

- Add 10-minute temperature statistics and compressor duty.
- Add rolling six-hour relay-pattern recording.
- Add pattern replay on temperature-measurement failure.
- Keep relay off and report fault if no complete valid pattern exists.

Exit criteria:

- Statistics match measured intervals.
- Invalid sensor periods are not learned.
- Replay preserves the recorded duty pattern while respecting compressor timing.

### Stage 6: ESP-NOW Plug Protocol

- Add Plug command/status packet types to `SGNode_Shared`.
- Plug uses fixed Base MAC.
- Base learns first valid Plug MAC and stores it in EEPROM.
- Implement command acknowledgement, packet validation, status reporting, and `Forget SGNode Plug`.

Exit criteria:

- Plug and Base exchange validated packets reliably.
- Base restart restores learned Plug MAC.
- Foreign Plug packets are ignored after learning.

### Stage 7: Outer PI Beer-Temperature Controller

- Implement requested beer target, internal target ramp, PI controller, and anti-windup on Plug.
- Add batch-size-based `Tn`.
- Add 3 K holding clamp, dynamic 5 K transition clamp, and fast-ramp behavior.
- Report PI state and derived air target to Base.

Exit criteria:

- Controller behaves correctly in simulation/Serial diagnostics.
- Mode transitions are bumpless.
- Output clamps and 1.0 C minimum air target work as specified.

### Stage 8: Base Batch And UI Integration

- Add Plug Auto Mode selection and ramp settings to Batch Wizard.
- Add Plug status, temperatures, controller state, and faults to Base UI.
- Add automatic cold-crash start policy.
- Keep Dry Hop and Package manual.

Exit criteria:

- Existing batches load safely.
- Plug-disabled operation remains unchanged.
- Plug-enabled batch sends correct beer targets and ramps.

### Stage 9: Shared CSV Integration

- Extend the existing batch CSV with Plug values and 10-minute compressor duty.
- Keep parser backward-compatible.
- Use latest known Plug status for Float-triggered rows.

Exit criteria:

- Old CSV files still load.
- New rows contain coherent Float and Plug values.
- No separate Plug log is created.

### Stage 10: Water Tests And Tuning

- Run controlled water tests at multiple fill levels.
- Validate `Kp`, batch-size-to-`Tn` scaling, delays, clamps, and ramp behavior.
- Tune parameters before using automatic control on an active fermentation.

Exit criteria:

- Stable holding performance.
- Controlled ramp performance without excessive overshoot.
- Cold-crash behavior respects configured ramp and temperature limits.

## Current Preferred Concept

Build an SGNode Plug firmware for an ESP8285 smart plug with two added temperature sensors.

Architecture:

- SGNode Base owns fermentation logic, batch state, and the requested beer-temperature profile.
- SGNode Plug owns both temperature measurements and the complete temperature-control loop.
- Base sends only the requested beer temperature, control mode, and profile-transition information via ESP-NOW.
- Plug derives its own fridge-air target from measured beer temperature and controls the fridge relay locally.
- Plug reports both temperatures, derived air target, controller state, and relay state back to Base.
- Base and Plug communication continues independently from Float packets.
- Float packets remain the primary trigger for combined batch CSV rows, but not for temperature-control availability.

Initial control strategy:

- Plug measures fridge air temperature with one DS18B20.
- Plug measures vessel/beer temperature through an immersion sleeve with a second DS18B20.
- When Plug Auto Mode is selected, Plug uses the immersion-sleeve temperature as its controlled process value.
- Float temperature remains available for diagnostics and logging, but is ignored by temperature control while Plug Auto Mode is active.
- Base sends the requested beer-temperature target from the active batch profile.
- Plug runs a slow outer beer-temperature controller that derives a fridge-air target.
- Plug runs an inner two-point fridge-air controller around that derived air target.
- Inner-loop thresholds: cooling on at air target + `Air on`, cooling off at air target + `Air off`.
- Current defaults are `Air off = 0.5 K` and `Air on = 1.1 K` to account for fridge aftercool.
- Plug sends average actual air temperature to Base every 10 minutes.
- Plug adapts its air-temperature target slowly based on the difference between immersion-probe temperature and requested beer target.
- Adaptive correction must be conservative, bounded, autonomous, and visible on Base through status reporting.

Action policy:

- Dry hop requires manual acknowledgement because SGNode cannot know whether hops were physically added.
- Remove dry hop requires manual acknowledgement or skip because SGNode cannot know whether hops were physically removed.
- Cold crash may auto-start when plug auto mode is enabled, the plug is reachable, all previous manual actions are resolved, and SGNode successfully commands the cold-crash target.
- Packaging remains manual.

## Prompt 1: Hardware Discovery

Inspect the ESP8285 smart plug hardware and document the pinout.

Known hardware:

- Smart plug: Gosund SP1
- MCU: ESP8285
- Jumper wires have already been soldered and routed outside the enclosure.

Confirmed external wiring:

| Wire color | Signal |
| --- | --- |
| Purple | RST |
| Blue | GPIO0 |
| Red | 3.3 V |
| Black | GND |
| White | ESP RX |
| Brown | ESP TX |
| Green | GPIO4, freed by removing the power-metering hardware |
| Yellow | GPIO5, freed by removing the power-metering hardware |

Planned sensor assignment:

| GPIO | Sensor role |
| --- | --- |
| GPIO4 | DS18B20 fridge-air sensor |
| GPIO5 | DS18B20 vessel/beer immersion-sleeve sensor |

GPIO4 and GPIO5 are intended as separate OneWire buses. Each bus receives its own pull-up resistor to 3.3 V.

Confirmed hardware modification:

- The power-metering hardware connected to GPIO4/GPIO5/GPIO12 has been desoldered.
- GPIO4 and GPIO5 are therefore available for the two DS18B20 buses.
- Plug power-consumption measurement is intentionally no longer available.
- Before connecting sensors, verify that GPIO4 and GPIO5 are electrically isolated from the removed metering circuit and switch cleanly between 0 V and 3.3 V.

Confirmed control pin behavior:

- Relay: GPIO14, active-high.
- Relay off: GPIO14 low.
- Relay on: GPIO14 high.
- Firmware must configure GPIO14 low as early as possible during boot.

Goals:

- Confirm the exact ESP8285 module/board revision used by this Gosund SP1.
- Use the confirmed Gosund SP1 assignment: GPIO14 relay, GPIO3 button, GPIO1/GPIO13 inverted LEDs.
- Identify safe flash pins and serial pads.
- Document mains-safety notes.
- Decide where an external temperature sensor can be physically routed.

Output:

- Add or update a hardware note for the plug.
- Include a pinout table.
- Include any unknowns that still need measurement.

## Prompt 2: Sensor Choice And Wiring

Wire and validate the two Plug temperature sensors.

Current decision:

- Use two DS18B20 temperature sensors.
- One sensor measures air temperature inside the fridge.
- One sensor measures vessel/beer temperature through an immersion sleeve.
- GPIO4 and GPIO5 are available, so each sensor uses its own OneWire bus.
- The original power-metering hardware has been removed to free GPIO4 and GPIO5.
- Sensors have been ordered.

Questions to resolve:

- Confirm a 4.7 kOhm pull-up from each sensor data line to 3.3 V.
- How to route sensor wiring without compromising electrical safety or enclosure integrity?

Sensor placement decision:

- Place the fridge-air sensor freely in the middle region of the refrigerator air volume.
- Keep it away from refrigerator walls, evaporator surfaces, direct cold-air outlets, and the fermentation vessel.
- Protect it from condensation and dripping water without thermally insulating it from the surrounding air.
- Fix its position so measurements remain comparable between batches.
- Insert the vessel/beer sensor fully to the end of the immersion sleeve.
- Use a small amount of food-safe thermal compound if needed for repeatable thermal contact.
- Secure and strain-relieve both sensor cables.
- Mark both cables clearly as `AIR` and `BEER`.
- Route sensor cables separately from mains-voltage and relay wiring wherever practical.

Placement validation:

- Verify that the air sensor responds substantially faster than the immersion-sleeve sensor during compressor cycling.
- Verify that the immersion-sleeve reading is stable and representative of vessel temperature.
- Record the installed positions so water tests and later batches remain comparable.

Output:

- Recommended sensor.
- Wiring table for both sensors.
- Pull-up requirements.
- Safety constraints.
- Sensor-role validation so swapped or faulty sensors can be detected.

## Prompt 3: ESP-NOW Protocol

Define the SGNode Base <-> SGNode Plug ESP-NOW protocol.

Base to Plug command fields:

- protocol version
- plug id
- requested beer target temperature C
- control mode: off, cooling, disabled/failsafe
- desired beer-temperature ramp rate or transition mode
- command sequence number
- timestamp or monotonic age reference

Plug to Base status fields:

- protocol version
- plug id
- current air temperature C
- current vessel/beer temperature C
- 10-minute average air temperature C
- 10-minute average vessel/beer temperature C
- minimum and maximum air temperature in report window
- minimum and maximum vessel/beer temperature in report window
- relay state
- compressor on-time ratio for the latest 10-minute report interval
- controller state
- derived fridge-air target
- active adaptive offset
- measured beer-temperature rate
- last command age
- uptime
- fault flags
- sequence acknowledgement

Sensor fault flags must distinguish:

- fridge-air sensor fault
- vessel/beer sensor fault
- implausible sensor-role swap or temperature relationship

Output:

- Add protocol structs to `SGNode_Shared`.
- Keep packet size small and fixed.
- Include versioning and sanity checks.

## Prompt 4: Plug Firmware Skeleton

Create a new Arduino sketch for the ESP8285 plug.

Suggested folder:

- `SGNode_Plug/SGNode_Plug.ino`

Core behavior:

- Boot with relay off.
- Initialize both DS18B20 sensors on separate OneWire buses.
- Initialize ESP-NOW.
- Register the known Base MAC as the Plug's fixed ESP-NOW peer.
- Accept control commands only from the configured Base MAC.
- Receive Base command packets.
- Run complete local cascaded temperature controller.
- Derive fridge-air target from requested beer target and immersion-probe feedback.
- Run local two-point air-temperature controller.
- Enforce compressor protection.
- Send status packets every 10 minutes.
- Send faster debug/status packets during startup if useful.

Safety behavior:

- Relay off until a valid command is received.
- Plug continues local control between Base updates.
- Plug stores the last valid derived fridge-air target.
- As long as fridge-air temperature remains plausible, Plug continues local air-temperature control using the last valid derived air target.
- Plug continuously records the compressor relay pattern for the last six hours while temperature measurements are valid.
- If temperature measurement is unavailable or implausible, Plug enters time-pattern fallback and repeats the last valid six-hour compressor pattern.
- Compressor minimum on/off protection remains active while replaying the pattern.

Output:

- Compileable ESP8285 firmware.
- No dependency on Wi-Fi router, MQTT, or web UI.

## Prompt 5: Local Plug Controller

Implement the local two-point fridge controller in the plug firmware.

Initial algorithm:

- Cooling on if `airTemp >= airTarget + Air on`.
- Cooling off if `airTemp <= airTarget + Air off`.
- Default thresholds: `Air off = 0.5 K`, `Air on = 1.1 K`.
- Enforce minimum compressor off time, default 5 minutes.
- Optional minimum on time, default 2 minutes.
- Relay must remain off when mode is disabled or sensor is invalid.

Control sensor roles:

- Local two-point relay control uses fridge-air temperature.
- Plug outer-loop/adaptive control uses vessel/beer temperature.
- Base provides only the requested beer-temperature target and phase/transition intent.
- A failed fridge-air sensor prevents safe local relay control.
- A failed vessel/beer sensor pauses the outer beer-temperature controller.
- When the vessel/beer sensor fails, Plug continues the inner air-temperature controller using the last valid derived air target, as long as fridge-air temperature remains plausible.
- Sensor failures and degraded air-only control are reported to Base.
- If fridge-air temperature also fails or becomes implausible, Plug stops temperature-based control and enters six-hour time-pattern fallback.

Time-pattern fallback:

- Maintain a rolling six-hour history of relay on/off state.
- Record history only while the required temperature measurements are valid and normal control is active.
- On temperature-measurement failure, repeat the most recent valid six-hour relay pattern.
- Continue enforcing minimum compressor off time and minimum compressor on time.
- Do not modify the adaptive air target while replaying the pattern.
- Report fallback state, pattern age, and replay position to Base.
- If no complete valid six-hour history exists, keep the compressor relay off and report the fault to Base.

Output:

- Controller function with unit-testable logic if practical.
- Clear state names for UI/status reporting.

Suggested controller states:

- `BOOT_SAFE_OFF`
- `WAITING_FOR_COMMAND`
- `IDLE_IN_BAND`
- `COOLING_ON`
- `MIN_OFF_TIME_LOCKOUT`
- `MIN_ON_TIME_HOLD`
- `SENSOR_FAULT`
- `AIR_ONLY_HOLD`
- `TIME_PATTERN_FALLBACK`

State outputs:

- Relay state.
- Reason why relay is on/off.
- Last valid command age.
- Sensor validity.
- Whether the controller is holding the last valid air target.
- Whether the controller is replaying a compressor pattern.
- Age of the recorded pattern and current replay position.

## Prompt 6: Base Integration

Integrate SGNode Plug support into SGNode Base.

Base responsibilities:

- Learn the Plug MAC from the first valid Plug status packet and persist it in EEPROM.
- Send requested beer temperature from the active batch profile via ESP-NOW.
- Send transition/ramp intent when the profile target changes.
- Show plug status on the display.
- Show fridge air temperature average.
- Show vessel/beer temperature from the Plug.
- Show derived fridge-air target and adaptive offset calculated by Plug.
- Show relay/cooling state.
- Show stale/failsafe warning if plug status is old.
- Auto-start cold crash when all guardrails are satisfied.

Output:

- Base compiles for ESP32 huge app partition.
- Plug control can be enabled/disabled from UI or config.
- Existing float communication remains unaffected.

## Prompt 6B: Batch Wizard Plug Setup

Extend batch creation to ask whether SGNode Plug temperature control should be used.

Wizard behavior:

- If a paired plug is available, offer Plug Auto Mode during batch creation.
- If a paired plug is reachable, default to Auto Mode but still show the choice.
- If no plug is reachable, show that the temperature profile will be advisory only.
- Store the chosen mode in the batch profile or associated batch settings.
- When Plug Auto Mode is enabled, identify Plug vessel/beer temperature as the authoritative temperature for Base display, logging, and batch analysis. Plug still performs the control loop locally.
- When Plug Auto Mode is enabled, allow selection of normal temperature-change and cold-crash ramp rates.
- Ramp-rate inputs must be constrained to sensible ranges.

Suggested UI text:

- `SGNode Plug found`
- `Use Plug Auto Mode?`
- `Temperature profile will control fridge`
- `Beer temperature source: Plug probe`
- `Normal ramp`
- `Cold crash ramp`
- `No plug found - profile display only`

Output:

- Batch stores whether plug control is enabled.
- Batch stores selected plug id/MAC if needed.
- Existing batches without plug settings load safely with plug control disabled.
- Batch profile stores the selected normal and cold-crash ramp rates.

Initial ramp-rate proposal for later validation:

| Ramp type | Default | Initial selectable clamp |
| --- | --- | --- |
| Normal fermentation temperature change | 0.2 K/h | 0.1-0.5 K/h |
| Cold crash | 1.0 K/h | 0.5-1.5 K/h |

Notes:

- The current observed cold-crash response was approximately 1.2 K/h at about 10 K air-to-vessel difference.
- Defaults and clamps should remain easy to revise after more real batch data is available.
- Plug receives the selected ramp rate with each relevant Base command and performs the ramp locally.

## Prompt 6C: Float-Ticked Plug Exchange

Use incoming Float packets as the main timing source for combined analysis and CSV logging, but keep Plug control communication independent.

Float-triggered cycle:

1. Base receives Float packet.
2. Base updates SG, Float diagnostics, fermentation state, and batch action state.
3. Base requests or reads latest Plug status.
4. Base computes the requested beer target from the batch temperature profile.
5. Base sends requested beer target and transition intent to Plug.
6. Plug independently computes its air target and relay state.
7. Base logs Float and latest Plug data in the same batch CSV row.

Notes:

- Plug must continue local control even if Float packets are delayed.
- Base sends Plug commands and heartbeats independently from Float packets, initially about every 1-2 minutes.
- Plug answers Base commands with current status.
- Plug additionally maintains and reports its 10-minute statistics.
- Base uses the latest known Plug status when writing the next Float-triggered CSV row.
- Plug status should include command sequence acknowledgement so Base can tell whether the latest target was accepted.

Output:

- Clear update order in Base.
- No direct dependency on Wi-Fi, MQTT, or router infrastructure.

## Prompt 6D: Shared CSV Logging

Add Plug values to the active batch CSV.

Preferred approach:

- Keep one CSV row per Float measurement.
- Add Plug-related columns to the existing row.
- Leave fields empty or zero/default when Plug is disabled or stale.
- Use only the active batch CSV; do not create a separate high-resolution Plug log.
- Store the compressor on-time ratio for the approximately 10-minute interval between data points instead of logging every relay transition.

Candidate new columns:

- `beer_target_temp`
- `beer_temp_source`
- `plug_beer_temp`
- `plug_beer_avg_10m`
- `plug_beer_min_10m`
- `plug_beer_max_10m`
- `float_temp`
- `fridge_air_target`
- `fridge_air_temp`
- `fridge_air_avg_10m`
- `fridge_air_min_10m`
- `fridge_air_max_10m`
- `fridge_relay_state`
- `fridge_compressor_duty_10m`
- `fridge_controller_state`
- `plug_last_seen_s`
- `plug_fault_flags`
- `plug_command_seq`
- `plug_ack_seq`

Questions:

- Whether to rename existing temperature columns for clarity.
- How to preserve compatibility with existing CSV readers.

Output:

- Updated CSV header.
- Updated CSV append code.
- Parser remains backward compatible with old CSV files.
- Compressor duty value is represented consistently, preferably as percent from 0-100.

## Prompt 7: Plug Cascaded Temperature Controller

Plan and implement the autonomous cascaded temperature controller on SGNode Plug.

Idea:

- Base sends the requested beer-temperature target.
- Plug compares immersion-sleeve beer temperature to the requested beer target.
- Plug uses a PI controller to calculate its internal fridge-air target offset.
- Plug uses fridge-air temperature and two-point hysteresis to control the relay.
- Adjustment must be slow because beer temperature is thermally lagged.

Possible guardrails:

- Update offset no faster than every 30-60 minutes.
- Use a dynamic offset clamp based on operating phase, desired beer-temperature ramp, and observed beer-temperature response.
- Ignore offset updates during active compressor transitions.
- Ignore offset updates if float data is stale.
- Prefer stable trend windows over single readings.

Output:

- Written algorithm proposal.
- Plug runtime flag to enable/disable adaptive outer-loop mode.
- Plug status reports requested beer target, measured beer temperature, derived air target, and adaptive offset.
- Base UI displays those reported values.

Outer PI controller concept:

- Controlled value: vessel/beer temperature from the immersion-sleeve DS18B20.
- Setpoint: requested beer temperature sent by Base.
- Controller output: fridge-air target offset relative to requested beer temperature.
- `beerError = plugBeerTemp - requestedBeerTarget`.
- Positive beer error requests a colder fridge-air target.
- Negative beer error requests a warmer fridge-air target or compressor inactivity.
- Current implementation: Base sends the active controller parameters with each Plug command.
- `Tn` is currently an explicit editable value from `/data/plug/gov_settings.json`.
- Batch-size-dependent `Tn` scaling remains a future refinement after more fill-level tests.
- Apply anti-windup whenever the controller output reaches its active offset clamp.
- Freeze or back-calculate the integral term while compressor operation cannot affect the requested direction.
- Filter beer temperature and rate before feeding the PI controller.
- Do not update the controller from single noisy readings.

PI output clamps:

- Holding mode: maximum air-to-beer target offset magnitude of 3 K.
- Controlled ramps up to and including 0.8 K/h: dynamic output clamp up to 5 K.
- Cooling ramps above 0.8 K/h: fast-ramp behavior with minimum fridge-air target of 1.0 C.

Deferred `Tn` scaling concept:

- Larger batches receive a longer `Tn`.
- Smaller batches receive a shorter `Tn`.
- Clamp `Tn` to a sensible minimum and maximum so invalid batch sizes cannot create unstable behavior.
- The exact batch-size-to-`Tn` relationship must be validated experimentally.
- Keep the relationship configurable rather than scattering constants through control logic.

Current tested controller defaults:

```text
Kp = 0.45
Tn = 0.75 h
D brake = 0.80 h
Air off = +0.50 K above air target
Air on = +1.10 K above air target
Beer undershoot lockout = 0.10 K
D max = 0.90 K
Warming D factor = 0.25
```

Future example `Tn` values if batch-size scaling is introduced:

| Batch size | Initial Tn |
| ---: | ---: |
| 5 L | 3.0 h, minimum clamp |
| 10 L | 4.6 h |
| 13 L | 6.0 h |
| 20 L | 9.2 h |
| 25 L | 11.5 h |
| 30 L | 12.0 h, maximum clamp |

Current reasoning:

- The first clean 13 L water/fridge validation showed the 9 C hold stable enough with these values.
- The fridge has noticeable aftercool after relay-off, so `Air off` and `Air on` are intentionally above the computed air target.
- The 4 C hold looked clean on the earlier SD data; additional fill levels and real fermentation heat are still needed before treating these as universal defaults.

Initial controller execution rules:

- Run the outer PI calculation once per minute.
- Use a filtered immersion-probe temperature. Current control uses the most recent short control average and a longer history for rate estimation.
- Use an error deadband of approximately +/- 0.1 K in holding mode.
- Ramp the beer setpoint internally rather than stepping directly to the final target.
- Apply conditional-integration anti-windup: do not integrate further when the output is clamped and the current error would drive it farther into saturation.
- Allow the integral term to unwind when the error direction moves the output away from saturation.
- Reset or safely initialize the integral term when starting a new batch or changing controller mode.
- Prefer bumpless transfer when switching between holding, controlled-ramp, fast-ramp, and air-only-hold modes.

Water-test plan for tuning:

- Test several fill levels, including approximately 5 L, 13 L, and the largest expected batch.
- At each fill level, measure response to known fixed air-to-water offsets such as 2 K, 3 K, and 5 K.
- Record delay before water temperature responds, stable cooling/heating rate, overshoot, compressor duty, and recovery after target arrival.
- Start with P-only tests where practical before enabling the integral term.
- Tune `Kp` first, then validate the batch-size-to-`Tn` relationship.
- Keep the initial linear `Tn` scaling unless test data clearly supports a different relationship.
- Do not tune using active fermentation as the first test because fermentation heat adds an uncontrolled disturbance.

Observed system response:

- During a manually started cold crash, approximately 10 K difference between fridge air and vessel/float temperature produced about 1.2 K per hour beer-temperature decrease.
- Treat this as an initial real-world data point, not yet as a fixed model constant.

Dynamic clamp concept:

- In steady holding mode, clamp the air-to-beer target offset to a maximum magnitude of 3 K.
- During a controlled cooling transition at or below 0.8 K/h, dynamically vary the permitted air-to-beer offset up to a maximum magnitude of 5 K.
- The dynamic offset should grow only as needed to achieve the requested beer-temperature ramp and shrink as the measured ramp approaches or exceeds the request.
- Reduce the permitted offset as the beer temperature approaches its target.
- Use measured float-temperature slope as feedback.
- Prevent the adaptive controller from continuing a large offset after the requested ramp has been reached.
- Keep absolute air-temperature safety limits separate from the dynamic offset clamp.

Fast cooling ramp decision:

- For commanded cooling ramps above 0.8 K/h, allow aggressive cooling without the normal dynamic air-to-beer offset clamp.
- During this fast-ramp mode, clamp the derived fridge-air target to a minimum of 1.0 C unless later tests justify a colder target.
- The normal asymmetric air thresholds remain active around that target.
- With a 1.0 C minimum air target and the current `Air off = 0.5 K`, measured fridge-air temperature should normally switch the compressor off around 1.5 C before aftercool.
- There is no separate 1.0 C measured-air hard shutdown in this mode.
- The refrigerator itself has an independent hard limiter at 0 C, providing the final physical low-temperature protection.
- SGNode Plug must not rely on the refrigerator limiter for normal regulation; it remains a last protection layer.
- Exit fast-ramp mode and restore normal dynamic clamping when the requested cooling ramp is 0.8 K/h or lower, or when the vessel temperature approaches its target.
- This rule applies to cooling ramps only; warming ramps do not request compressor cooling.

Candidate transition relationship for later validation:

```text
observed 10 K air/beer offset -> approximately 1.2 K/h beer cooling
initial estimated gain -> approximately 0.12 (K/h) per K offset
```

This relationship should be learned or validated over multiple transitions before it controls the full allowed offset.

Adaptive update guardrails:

- Both local Plug sensors are valid and fresh.
- Plug Auto Mode command from Base is valid and fresh enough.
- Compressor has not just switched state.
- System is not in sensor fault.
- During cold crash and other transitions, switch from steady-state offset control to ramp-aware dynamic clamp control.

State to expose:

- `beerTargetTempC`
- `beerActualTempC`
- `beerTempSource`
- `floatTempDiagnosticC`
- `fridgeAirTempC`
- `airTargetBaseC`
- `adaptiveOffsetC`
- `airTargetCommandedC`
- `adaptiveEnabled`
- `adaptiveLimited`
- `piKp`
- `piTn`
- `piIntegral`
- reason when adaptation is paused

## Prompt 7A: Temperature Control State Model

Define the temperature-control state shared between Base UI, Base logs, and Plug status.

Base-side states:

- `TEMP_CONTROL_OFF`
- `TEMP_PROFILE_ADVISORY`
- `TEMP_AUTO_WAITING_FOR_PLUG`
- `TEMP_AUTO_ACTIVE`
- `TEMP_AUTO_STALE`
- `TEMP_AUTO_FAULT`
- `TEMP_AUTO_COLD_CRASH`

Plug-side states:

- `PLUG_OFF`
- `PLUG_WAITING_FOR_COMMAND`
- `PLUG_IDLE_IN_BAND`
- `PLUG_COOLING`
- `PLUG_LOCKOUT_MIN_OFF`
- `PLUG_HOLD_MIN_ON`
- `PLUG_SENSOR_FAULT`
- `PLUG_AIR_ONLY_HOLD`
- `PLUG_TIME_PATTERN_FALLBACK`

Important distinctions:

- Base decides fermentation and target profile state.
- Base sends the requested beer-temperature target but does not calculate the air target.
- Plug decides derived air target, adaptive offset, and immediate relay state.
- Plug immersion probe is the process measurement used by the Plug controller.
- Float temperature remains logged as a secondary diagnostic measurement.
- Cold crash auto-start is a Base action that sends a lower target to Plug.
- Plug never decides batch actions such as dry hop, cold crash, or package.

Output:

- State enum definitions.
- Mapping to display labels.
- Mapping to CSV fields.
- Fault/stale behavior rules.

## Prompt 8: Testing Plan

Create a test plan for the plug and Base integration.

Tests:

- Relay starts off after boot.
- Relay remains off without valid command.
- Relay turns on above upper threshold.
- Relay turns off below lower threshold.
- Compressor minimum off time is respected.
- Sensor failure forces relay off.
- Fridge-air sensor and vessel/beer sensor faults are reported separately.
- Failed vessel/beer sensor pauses adaptive control but continues air control at the last valid air target.
- Failed or implausible fridge-air temperature enters six-hour time-pattern fallback.
- Time-pattern fallback repeats the last valid relay pattern.
- Time-pattern fallback still respects compressor minimum on/off times.
- Invalid sensor periods are not learned into the fallback pattern.
- Temperature failure without a complete valid six-hour pattern keeps the relay off and reports a fault to Base.
- Cooling ramps above 0.8 K/h enter fast-ramp mode without the normal dynamic offset clamp.
- Fast-ramp mode never commands a fridge-air target below 1.0 C.
- With the current asymmetric thresholds, fast-ramp mode still turns the compressor off above the air target to compensate for aftercool.
- Refrigerator hard limiter at 0 C remains independent and is not part of the normal Plug control algorithm.
- Holding mode clamps air-to-beer target offset to 3 K.
- Controlled cooling ramps at or below 0.8 K/h dynamically clamp air-to-beer target offset to at most 5 K.
- Larger batch sizes select a longer PI integral time than smaller batch sizes.
- Invalid batch sizes cannot select a `Tn` outside configured limits.
- PI integral anti-windup works at the 3 K, 5 K, and fast-ramp output limits.
- Current controller defaults are `Kp = 0.45`, `Tn = 0.75 h`, and `D brake = 0.80 h`.
- Batch-size-based `Tn` scaling is deferred until more fill-level data supports it.
- Controller uses filtered beer temperature and holding deadband.
- Mode changes do not create a sudden unsafe air-target jump.
- Plug Auto Mode selects immersion-probe temperature as authoritative for Base display, logging, and batch analysis.
- Disabling Plug Auto Mode restores Float temperature as the Base-side beer-temperature source.
- Float temperature remains logged while ignored by temperature control.
- Status packet reports average/min/max over 10 minutes.
- Status packet and shared CSV report compressor duty ratio for the latest 10-minute interval.
- Base displays stale plug warning.
- Existing Float ESP-NOW packets still work.

## Prompt 6A: Action Automation Policy

Define which batch actions can be automatically acknowledged by SGNode when Plug auto mode is enabled.

Policy:

- `Dry Hop`: manual only.
- `Remove Dry Hop`: manual or skip only.
- `Cold Crash`: auto-start allowed.
- `Package`: manual only.

Cold crash auto-start guardrails:

- Plug auto mode is enabled for the batch.
- Plug status is fresh.
- Plug has accepted the latest command or status confirms the requested mode.
- Dry hop is either disabled, skipped, or done and removed/skipped.
- Fermentation state indicates final gravity stable or ready for cold crash.
- Cold crash has not already been done or skipped.

Cold crash auto-start behavior:

- Set `coldCrashDone` or equivalent started state according to the existing profile model.
- Set `coldCrashStartedAt`.
- Append an event such as `COLD_CRASH_AUTO_STARTED`.
- Send the cold-crash air-temperature target to the plug.
- Display that cold crash was auto-started by SGNode.

Open design note:

- The current profile model uses `coldCrashDone` as the action acknowledgement flag. Decide whether this should mean "cold crash started" or whether a separate `coldCrashStarted` state is needed before implementation.

Output:

- Manual test checklist.
- Serial diagnostics commands if needed.
- Arduino compile commands for Base and Plug.

## Pairing Decision

- The SGNode Base MAC address is known before flashing SGNode Plug.
- The Base MAC is compiled or configured into Plug firmware.
- Plug registers this fixed Base MAC as its ESP-NOW peer.
- Plug accepts commands only from the configured Base MAC.
- No button-based Plug pairing workflow is required for the first version.
- Base learns the sender MAC from the first valid Plug status packet.
- Base stores the learned Plug MAC in EEPROM and restores it after reboot.
- After a Plug MAC has been learned, Base accepts Plug status only from that MAC.
- Base must validate packet type, protocol version, size, and checksum before learning the sender MAC.
- Base provides a confirmed menu action named `Forget SGNode Plug`.
- After confirmation, Base deletes the persisted Plug MAC from EEPROM.
- The next valid Plug status packet is learned automatically as the new Plug.
- While a Plug MAC is stored, valid-looking status packets from other MAC addresses are ignored.
- Forget/relearn actions should be written to diagnostics or event output.

## Open Questions

- Verify GPIO4/GPIO5 electrical isolation after removing the power-metering hardware.
- Validate the current controller defaults on other fill levels and a real fermentation.
- Decide whether explicit `Tn` remains a global editable setting or becomes batch-size-derived with an editable multiplier.

## Deferred Topics

- Float-temperature fallback when the immersion probe fails or is displaced.
- Immersion-probe displacement detection.
