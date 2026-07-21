# SGNode Base Firmware

## OTA Update

Base OTA is a manual maintenance mode.

1. Open the bottom `More` tab.
2. Tap `OTA Update`.
3. Confirm `Start Base OTA?`.
4. Connect to the SoftAP shown on the display, for example `SGNode-Base-OTA-xxxxxx`.
5. Open `http://192.168.4.1/` and upload the new `.bin`.

During OTA the base pauses normal ESP-NOW operation. The OTA mode stays active while a client is connected. If no client connects for 180 seconds, the base reboots back into normal operation.

The OTA page also provides compact diagnostics at `/diag` and a `Reboot normal mode` link.

## Display Brightness

The `More` tab has a `Brightness` item. It cycles through `Low`, `Mid`, `High`, and `Max` using PWM on GPIO27, which is the LCD backlight pin on the ESP32 display board.
