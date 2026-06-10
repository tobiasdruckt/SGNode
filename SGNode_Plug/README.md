# SGNode Plug firmware

Target hardware: Gosund SP1 with ESP8285 and removed power-measurement circuit.

## Pin assignment

| GPIO | Function |
| --- | --- |
| 1 | Secondary LED |
| 3 | Button |
| 4 | Fridge-air DS18B20 data |
| 5 | Beer/vessel DS18B20 data |
| 13 | Primary LED |
| 14 | Relay, active high |

Each DS18B20 uses a separate OneWire bus and needs a 4.7 kOhm pull-up from
its data line to 3.3 V.

The relay is initialized OFF before any controller logic runs.

## Control behavior

- The Base sends beer target, ramp rate, and batch size by ESP-NOW.
- The Plug averages the beer probe over up to ten minutes and updates its
  outer PI controller once per minute.
- The outer PI controller derives an air target. The inner controller switches
  the relay with a total hysteresis of 0.8 K.
- If the beer probe fails while the air probe remains valid, the last air
  target is held.
- If the air probe fails, a complete learned six-hour compressor pattern is
  replayed. Without a complete pattern, the relay remains off.
- Every Plug status contains both temperatures, targets, controller state,
  faults, relay state, and the rolling ten-minute duty cycle.

The Base learns the MAC of the first valid Plug status packet and stores it in
EEPROM. Plug control remains disabled by default and can be enabled per batch
with the Brew Wizard `SGNode Plug` step.

## Build target

Use `Generic ESP8266 Module`, 1 MB flash, DOUT flash mode, and 80 MHz CPU.

## Bench-test order

The low-level bench-test sketches are intentionally local-only and ignored by
Git. Keep them beside the repo when working on hardware, but do not publish
them as part of the main firmware tree.

1. Flash local `SGNode_Plug_Hardware_Check` and verify the serial pin report.
2. With no 230 V connection, flash local `SGNode_Plug_Sensor_Relay_Test`.
3. Verify both temperatures on serial and relay continuity with a multimeter.
4. Flash `SGNode_Plug` only after the sensor assignment and relay polarity are
   confirmed.
