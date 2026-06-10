# SGNode Float Firmware

**Release:** alpha 0.5.0  
**Role:** Battery-powered ESP32 sensor float

The float measures tilt, temperature, and battery voltage, converts tilt to specific gravity using stored calibration coefficients, and sends packets to the base station via ESP-NOW.

## Hardware

- WeMos D32 ESP32 board with 18650 holder
- BMI160 IMU on I2C
- BMP180 temperature sensor on I2C
- Battery voltage divider on GPIO15
- Calibration switch on GPIO12
- Status LEDs on GPIO5 and GPIO16

## Main Behavior

1. Wake from sleep.
2. Read battery before Wi-Fi activity.
3. Initialize sensors.
4. Collect filtered tilt and temperature readings.
5. Calculate specific gravity.
6. Send ESP-NOW payload with protocol version and CRC.
7. Return to deep sleep.

Calibration switch ON keeps the float awake for the base station calibration workflow.

## Required Libraries

- EmotiBit BMI160
- Adafruit BMP085
- Wire
- EEPROM

## Configuration

Set the base station MAC address in `SGNode_Float.ino`:

```cpp
uint8_t baseStationMac[] = {0xA4, 0xF0, 0x0F, 0x68, 0x22, 0x00};
```

The shared payload format is defined in `../SGNode_Shared/sg_protocol.h`.

## Calibration

Calibration coefficients are stored in EEPROM. The base station sends calibration commands while the float is in calibration mode.

Current behavior:
- Sensor offset calibration is supported.
- Multiple SG points are recorded.
- Polynomial coefficients are calculated on the float.
- EEPROM versioning is used for backward compatibility.

Adding extra optional calibration points normally requires flashing the float because the point handling, polynomial fit, and EEPROM format live in the float firmware.

## Power Notes

- Deep sleep is the expected normal state.
- Calibration mode has high drain because the float stays awake.
- Battery measurement uses GPIO15 with a calibrated voltage divider.
- If battery drain is high, first confirm that the float is not staying in calibration mode and that the sleep interval is actually reached after each send.

## Troubleshooting

- No packets: check base station MAC and ESP-NOW channel.
- Wrong SG: recalibrate and verify the float is mechanically stable.
- Wrong temperature: check BMP180 wiring and I2C sharing.
- Battery reads wrong: verify the divider and calibration factor.
- Fast drain: inspect deep sleep entry and sensor power behavior.
