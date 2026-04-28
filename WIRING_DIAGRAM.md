# Hardware Wiring Diagrams

## Float Unit (Sensor) Wiring

### WeMos D32 ESP32 Board with Built-in Battery Support

The float unit uses the **WeMos WiFi&Bluetooth Battery (D32)** board which includes:
- Built-in 18650 battery holder
- Built-in charging circuit (500mA max)
- Built-in power switch
- Over charge/discharge protection
- Battery monitoring via ADC (GPIO35)
- LED_BUILTIN (GPIO5)
- Extra programmable LED (GPIO16)

### BMI160 IMU Connections
```
BMI160 Sensor    WeMos D32
------------    ---------
VCC        →    3.3V
GND        →    GND
SDA        →    GPIO21 (I2C SDA)
SCL        →    GPIO22 (I2C SCL)
```

### GY-68 BMP180 Temperature/Pressure Sensor Connections
```
BMP180 Sensor    WeMos D32
-------------    ---------
VCC         →    3.3V
GND         →    GND
SDA         →    GPIO21 (I2C SDA)  [Shared with BMI160]
SCL         →    GPIO22 (I2C SCL)  [Shared with BMI160]
```

**Note:** The BMP180 shares the same I2C bus as the BMI160. Both sensors can be connected simultaneously as they have different I2C addresses (BMI160: 0x68, BMP180: 0x77).

### Calibration Switch Connection
```
SPST Switch    WeMos D32
-----------    ---------
Pin 1      →    GPIO12 (INPUT_PULLUP)
Pin 2      →    GND
```

**Note:** When switch is closed (GPIO12 = LOW), float enters calibration mode. The WeMos D32's built-in power switch controls main power.

### LED Indicators (WeMos D32 Built-in)
```
LED_BUILTIN (GPIO5)   → Available for user programming
LED_EXTRA (GPIO16)    → Calibration mode indicator (ON = calibration active)
```

**Note:** The WeMos D32 also has built-in charging LEDs (Green = full, Red = charging) that are not programmable.

## Base Station Wiring

### 4.0inch ESP32-32E Display (Integrated Module)
```
4.0inch ESP32-32E Display Module
--------------------------------
- Integrated ESP32 microcontroller
- Integrated ST7796 display driver (320x240)
- Integrated resistive touch screen
- Built-in battery charging circuit
- Built-in ESP-NOW antenna

Connections:
- USB Port → 5V power supply for charging and operation
- No external wiring required for display or touch
- ESP32, display, and touch screen are pre-connected on module

Pin Functions (for reference):
- GPIO5: Display CS (Chip Select)
- GPIO4: Display DC (Data/Command)
- GPIO2: Display RESET
- GPIO15: Touch CS
- GPIO27: Touch IRQ
- GPIO23: SPI MOSI (shared)
- GPIO18: SPI SCK (shared)
- GPIO19: SPI MISO (shared)
```

## Power Requirements

### Float Unit Power (WeMos D32)
- 18650 Li-ion battery (3.7V nominal, 4.2V max) - built-in holder
- Built-in charging circuit (500mA max, USB-C input)
- Built-in power switch
- Over charge/discharge protection
- Deep sleep current: ~0.1mA
- Active current: ~50-100mA
- Expected battery life: 17+ hours continuous, weeks with deep sleep

### Base Station Power
- 5V USB power supply
- Display current: ~230mA (display only)
- Total current: ~290mA (display + charging)
- USB power recommended for stable operation
```

## Complete Connection Summary

### Float Unit Pin Map (WeMos D32)
```
WeMos D32 Pin    Connection
---------------    ----------
GPIO21            BMI160 SDA, BMP180 SDA
GPIO22            BMI160 SCL, BMP180 SCL
GPIO35            Battery Voltage (built-in ADC)
GPIO12            Calibration Switch (INPUT_PULLUP)
GPIO5             LED_BUILTIN (programmable)
GPIO16            LED_EXTRA (calibration indicator)
3.3V              BMI160 VCC, BMP180 VCC, Pull-ups
GND               BMI160 GND, BMP180 GND, Switch
Built-in          18650 Battery (built-in holder)
Built-in          Charging circuit (USB-C)
Built-in          Power switch
```

### Base Station Pin Map
```
4.0inch ESP32-32E Display Module
--------------------------------
All pins are pre-connected on the integrated module.
No external wiring required for display or touch screen.

Key internal connections (for reference):
- GPIO5: Display CS (Chip Select)
- GPIO4: Display DC (Data/Command)
- GPIO2: Display RESET
- GPIO15: Touch CS
- GPIO27: Touch IRQ
- GPIO13: SD Card CS (Chip Select)
- GPIO23: SPI MOSI (shared with display/SD)
- GPIO18: SPI SCK (shared with display/SD)
- GPIO19: SPI MISO (shared with display/SD)

External connections:
- USB Port: 5V power supply and programming
- No other external connections needed
```

## Component Specifications

### Required Components

**Float Unit:**
- WeMos D32 ESP32 Board with built-in 18650 battery holder
- BMI160 IMU Sensor Module
- GY-68 BMP180 Temperature/Pressure Sensor
- 18650 Li-ion Battery (2000mAh+ recommended, e.g., LG 3000mAh)
- SPST Switch (for calibration mode)
- Waterproof enclosure

**Base Station:**
- 4.0inch ESP32-32E Display Module (integrated ESP32 + ST7796 display)
- 5V USB Power Supply (1A+ recommended)
- USB cable for power and programming

### Power Requirements

**Float Unit:**
- Operating voltage: 3.0V - 4.2V (built-in battery)
- Sleep current: <1mA
- Active current: <200mA
- Expected battery life: 17+ hours continuous, weeks with deep sleep

**Base Station:**
- Operating voltage: 5V (USB supply)
- Current consumption: ~290mA (display + charging)
- Power consumption: ~1.45W

## Assembly Notes

### Float Unit Assembly
1. **Waterproofing**: Ensure all connections are sealed
2. **Weight Distribution**: Place battery at bottom for stability
3. **IMU Orientation**: Mount BMI160 with axes aligned to float
4. **Antenna**: Keep ESP32 antenna area free from metal

### Base Station Assembly
1. **Module Setup**: 4.0inch ESP32-32E Display is pre-assembled
2. **Power Connection**: Connect USB cable to 5V power supply
3. **Programming**: Use USB port for uploading firmware
4. **Mounting**: Display module can be mounted in enclosure or used standalone

## Troubleshooting Wiring Issues

### Common Problems

**I2C Communication Issues (Float):**
- Check 4.7kΩ pull-up resistors on SDA/SCL
- Verify BMI160 address (0x68 or 0x69)
- Ensure 3.3V power is stable

**Display Issues (Base):**
- Verify TFT_eSPI library is configured for ST7796
- Check USB power supply is providing adequate current (500mA+)
- Ensure display module is properly powered via USB

**Touch Issues (Base):**
- Touch screen is integrated on 4.0inch ESP32-32E module
- Touch may require calibration in software
- Check touch screen is clean and responsive

**Power Issues:**
- Check battery voltage with multimeter
- Verify WeMos D32 built-in charging circuit
- Ensure built-in power switch is ON
- Check battery level via LED_EXTRA indicator

## Testing Procedure

### Float Unit Testing
1. Power on via USB (bypass deep sleep for testing)
2. Open Serial Monitor (115200 baud)
3. Verify BMI160 initialization
4. Test IMU readings by tilting the device
5. Verify battery voltage reading
6. Test ESP-NOW transmission

### Base Station Testing
1. Connect USB power supply to 4.0inch ESP32-32E module
2. Verify display initialization and screen output
3. Test touch screen functionality
4. Verify ESP-NOW initialization (check MAC address)
5. Test with float unit transmission
6. Verify data display and graph functionality

## Safety Considerations

### Electrical Safety
- Use proper insulation for all connections
- Verify voltage ratings before connecting
- Use current limiting for LED connections
- Ensure proper grounding

### Battery Safety
- Use protected 18650 batteries
- Never short-circuit battery terminals
- Use WeMos D32 built-in charging circuit (500mA max)
- Monitor battery temperature during charging
- Built-in over charge/discharge protection

### Water Safety (Float Unit)
- Ensure complete waterproofing
- Use marine-grade sealants
- Test waterproofing before deployment
- Provide moisture indicators for maintenance
