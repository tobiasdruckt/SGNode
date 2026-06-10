# SGNode Wiring Reference

**Current release:** alpha 0.5.0

## Float Unit

### I2C Sensor

```text
BMI160 VCC  -> 3.3V
BMI160 GND  -> GND
BMI160 SDA  -> GPIO26
BMI160 SCL  -> GPIO27
```

The BMP180/GY-68 was removed from the current float hardware. The firmware uses BMI160 chip temperature as a diagnostic value only.

### Battery Measurement

```text
Battery+ -> 51k -> GPIO15 -> 51k -> GND
                    |
                   10 uF
                    |
                   GND
```

GPIO15 is used as ADC input for battery voltage.

### Calibration Switch

```text
GPIO12 -> switch -> GND
```

Closing the switch enters calibration mode. Calibration mode keeps the float awake.

### LEDs

```text
GPIO5   -> built-in/status LED
GPIO16  -> calibration indicator LED
```

## Base Station

The 4.0-inch ESP32-32E display module has display, touch, and SD wiring integrated.

### Internal Reference Pins

```text
Display CS  -> GPIO5
Display DC  -> GPIO4
Display RST -> GPIO2
Touch CS    -> GPIO15
Touch IRQ   -> GPIO27
SD CS       -> GPIO13
SPI MOSI    -> GPIO23
SPI MISO    -> GPIO19
SPI SCK     -> GPIO18
```

### Optional RTC

```text
RTC SDA -> GPIO32
RTC SCL -> GPIO25
RTC VCC -> 3.3V
RTC GND -> GND
```

## Power

```text
Float: 18650 Li-ion in WeMos D32 holder
Base:  5V USB supply, 2A recommended
```

## Quick Checks

- Float BMI160 should be on GPIO26/GPIO27.
- Base station SD card must be FAT32.
- Float calibration switch must be open for normal low-power operation.
- Keep ESP32 antennas away from metal and liquid shielding where possible.
