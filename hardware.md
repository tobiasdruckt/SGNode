# SGNode Hardware Notes

**Current release:** alpha 0.4.0

## Bill Of Materials

### Float Unit

| Part | Qty | Notes |
| --- | ---: | --- |
| WeMos D32 ESP32 board | 1 | Built-in 18650 holder and charger |
| BMI160 IMU | 1 | Tilt measurement |
| BMP180 / GY-68 | 1 | Temperature measurement |
| 18650 Li-ion cell | 1 | 2000 mAh or larger recommended |
| SPST switch | 1 | Calibration mode on GPIO12 |
| Waterproof enclosure | 1 | Must fit fermenter opening |

### Base Station

| Part | Qty | Notes |
| --- | ---: | --- |
| 4.0-inch ESP32-32E display module | 1 | Integrated ESP32, ST7796 display, touch |
| MicroSD card | 1 | FAT32, 4 GB or larger |
| 5V USB power supply | 1 | 2 A recommended |
| DS323x-compatible RTC | 1 | Optional but recommended for stable timestamps |

## Float Pin Summary

| Signal | GPIO | Notes |
| --- | ---: | --- |
| I2C SDA | 21 | BMI160 and BMP180 |
| I2C SCL | 22 | BMI160 and BMP180 |
| Battery ADC | 15 | 51k/51k divider plus capacitor |
| Calibration switch | 12 | Switch to GND |
| Built-in LED | 5 | Status |
| Extra LED | 16 | Calibration indicator |

## Base Pin Summary

The 4-inch base module has display, touch, and SD wiring integrated. Current firmware also supports an RTC:

| Signal | GPIO |
| --- | ---: |
| RTC SDA | 32 |
| RTC SCL | 25 |
| SD CS | 13 |
| Display CS | 5 |
| Touch CS | 15 |

## Assembly Notes

- Keep the float antenna area clear of metal.
- Mount the IMU rigidly; movement inside the enclosure will ruin calibration.
- Put the battery low in the float for stable orientation.
- Seal the float enclosure before real fermentation use.
- Keep SD card access available on the base enclosure.
- Use a stable 5V supply for the base station; display and SD writes need margin.

## Power Notes

### Float

- Normal operation should spend most time in deep sleep.
- Calibration mode keeps the float awake and drains the battery quickly.
- If drain is high, measure sleep current directly and confirm sensor shutdown behavior.

### Base

- Designed for powered operation, not battery operation.
- Typical draw depends heavily on display backlight.
- SD access is mounted only when needed to reduce SPI/touch conflicts.

## Enclosure Targets

### Float

- PETG or ABS preferred.
- Waterproof target: IP67 style sealing.
- Stable buoyancy and consistent tilt response are more important than minimum size.

### Base

- Desktop stand or wall mount.
- Access for USB and MicroSD.
- Passive ventilation for ESP32 and display driver.
