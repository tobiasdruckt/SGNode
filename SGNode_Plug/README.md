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
- The Plug keeps a rolling beer-probe history and updates its outer controller
  once per minute.
- The outer PI/D-brake controller derives an air target. The inner controller
  switches the relay around that target with asymmetric thresholds above the
  air target. Current defaults switch off at air target + 0.5 K and on at air
  target + 1.1 K to compensate for fridge aftercool.
- If the beer probe fails while the air probe remains valid, the last air
  target is held.
- If the air probe fails, a complete learned six-hour compressor pattern is
  replayed. Without a complete pattern, the relay remains off.
- Every Plug status contains both temperatures, targets, controller state,
  faults, relay state, and the rolling ten-minute duty cycle.

Controller parameters are sent by Base with each command. Base stores the
editable defaults on the SD card in `/data/plug/gov_settings.json`.

The Base learns the MAC of the first valid Plug status packet and stores it in
EEPROM. Plug control remains disabled by default and can be enabled per batch
with the Brew Wizard `SGNode Plug` step.

## OTA update mode

Hold the Plug button for 5 seconds to enter OTA update mode. The relay is
switched off immediately and stays off. Normal ESP-NOW control is paused, and
the Plug starts an open SoftAP named `SGNode-Plug-OTA-<ChipId>`.

Connect to the AP and open:

- `http://192.168.4.1/`
- `http://192.168.4.1/update`
- `http://192.168.4.1/diag`

The root/update page contains a minimal firmware upload form, a normal-mode
reboot button, and compact diagnostics. After a successful upload the Plug
reboots. OTA mode remains active while a client is connected to the SoftAP. If
no upload is in progress and no client is connected for 180 seconds, OTA mode
times out and reboots so ESP-NOW starts cleanly again. Holding the hardware
button for 5 seconds while already in OTA mode also reboots to normal mode.

During OTA both LEDs blink. The secondary LED is on GPIO1/UART TX and may also
flicker when serial output is active.

## Build target

Use `Generic ESP8266 Module`, 1 MB flash, DOUT flash mode, and 80 MHz CPU.

Required libraries:

- OneWire
- DallasTemperature

The shared headers in `SGNode_Shared` are used as a local project library when
compiling the Plug.

## Bench-test order

The low-level bench-test sketches are intentionally local-only and ignored by
Git. Keep them beside the repo when working on hardware, but do not publish
them as part of the main firmware tree.

1. Flash local `SGNode_Plug_Hardware_Check` and verify the serial pin report.
2. With no 230 V connection, flash local `SGNode_Plug_Sensor_Relay_Test`.
3. Verify both temperatures on serial and relay continuity with a multimeter.
4. Flash `SGNode_Plug` only after the sensor assignment and relay polarity are
   confirmed.
