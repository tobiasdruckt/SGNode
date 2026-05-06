# Hardware Documentation

## Bill of Materials (BOM)

### Float Unit

| Component | Quantity | Notes | Source |
|-----------|----------|-------|--------|
| WeMos D32 ESP32 Board | 1 | Built-in 18650 battery holder | Amazon / AliExpress |
| BMI160 IMU Sensor | 1 | 6-axis accelerometer/gyroscope | Amazon / AliExpress |
| GY-68 BMP180 Sensor | 1 | Temperature/pressure sensor | Amazon / AliExpress |
| 18650 Li-ion Battery | 1 | 2000mAh+ recommended | Amazon / electronics store |
| SPST Switch | 1 | For calibration mode | Electronics store |
| Jumper Wires | Various | For connections | Electronics store |
| PCB / Prototyping Board | 1 | Optional, for neat wiring | Electronics store |

### Base Station

| Component | Quantity | Notes | Source |
|-----------|----------|-------|--------|
| 4.0inch ESP32-32E Display | 1 | Integrated ESP32 + ST7796 display + touch | LilyGO / AliExpress |
| MicroSD Card | 1 | 4GB+ for data logging | Electronics store |
| 5V Power Supply | 1 | USB or external (2A+) | Electronics store |
| USB-C Cable | 1 | For power/uploading | Electronics store |

## 3D Printed Parts

### Float Unit Enclosure
- **Status**: Design pending
- **Purpose**: Waterproof housing for float unit
- **Features to include**:
  - Compartment for WeMos D32 board
  - Mounting points for sensors
  - Battery compartment access
  - Calibration switch access
  - Waterproof seal design
  - Float attachment point

### Base Station Display Enclosure
- **Status**: Design pending
- **Purpose**: Protective housing for 4.0inch display
- **Features to include**:
  - Display window with touch access
  - Mounting stand/bracket
  - Cable management
  - Ventilation for ESP32
  - SD card access slot

## Wiring Diagrams

### Float Unit Connections
```
WeMos D32 ESP32 Board:
├── BMI160 IMU (I2C)
│   ├── VCC → 3.3V
│   ├── GND → GND
│   ├── SDA → GPIO 26
│   └── SCL → GPIO 27
├── BMP180 (I2C)
│   ├── VCC → 3.3V
│   ├── GND → GND
│   ├── SDA → GPIO 26 (shared with BMI160)
│   └── SCL → GPIO 27 (shared with BMI160)
├── Calibration Switch
│   ├── One side → GPIO 12
│   └── Other side → GND
└── Battery
    └── Built-in 18650 holder on WeMos D32
```

### Base Station Connections
```
4.0inch ESP32-32E Display:
├── Built-in ST7796 Display (SPI)
├── Built-in XPT2046 Touchscreen (SPI)
├── MicroSD Card Slot (SPI)
└── USB-C for power and programming
```

## Assembly Notes

### Float Unit Assembly
1. Mount WeMos D32 in enclosure
2. Connect BMI160 and BMP180 via I2C (shared bus)
3. Install calibration switch on GPIO 12
4. Install 18650 battery in holder
5. Test all connections before sealing enclosure
6. Ensure waterproof seal is properly installed

### Base Station Assembly
1. The 4.0inch ESP32-32E Display is an all-in-one unit
2. Simply connect via USB-C for power
3. Insert MicroSD card for data logging
4. Mount in 3D printed enclosure when available

## Power Requirements

### Float Unit
- **Operating Voltage**: 3.3V (regulated by WeMos D32)
- **Battery**: 18650 Li-ion (3.7V nominal)
- **Expected Battery Life**: 2-4 weeks with 2000mAh battery (depends on measurement interval)
- **Charging**: Built-in TP4056 charging circuit on WeMos D32

### Base Station
- **Operating Voltage**: 5V via USB-C
- **Current Draw**: ~200-500mA (display dependent)
- **Power Supply**: 5V 2A+ recommended

## Sensor Specifications

### BMI160 IMU
- **Type**: 6-axis IMU (3-axis accelerometer + 3-axis gyroscope)
- **Interface**: I2C
- **Accelerometer Range**: ±2g, ±4g, ±8g, ±16g
- **Gyroscope Range**: ±125°/s to ±2000°/s
- **I2C Address**: 0x68

### BMP180
- **Type**: Temperature and Pressure Sensor
- **Interface**: I2C
- **Temperature Range**: -40°C to +85°C
- **Pressure Range**: 300 to 1100 hPa
- **I2C Address**: 0x77

## Enclosure Design Guidelines

### Float Unit Enclosure
- **Material**: PETG or ABS (water-resistant)
- **Wall Thickness**: 2-3mm
- **IP Rating**: IP67 target
- **Size**: Compact, fits standard fermenter opening
- **Weight**: Neutral buoyancy when partially filled

### Base Station Enclosure
- **Material**: PLA or PETG
- **Wall Thickness**: 2-3mm
- **Design**: Desktop stand or wall mount option
- **Access**: Easy SD card and USB access
- **Ventilation**: Passive cooling vents for ESP32
