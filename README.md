# SGNode - Fermentation Monitoring System

**Version: 0.3.0-alpha**

A robust two-part ESP32-based system for monitoring fermentation progress by measuring the tilt of a floating sensor device. Features real-time data display, calibration system, and comprehensive data logging.

## System Overview

### Float Unit (Sensor)
- Battery-powered autonomous sensor with WeMos D32 board
- Measures tilt using BMI160 IMU sensor with advanced 3D tilt calculation
- Measures temperature using BMP180 sensor with EMA filtering
- Transmits data via ESP-NOW protocol with CRC verification
- Deep sleep for power efficiency (weeks of battery life)
- Built-in 18650 battery holder with charging circuit
- LED indicators for status and calibration mode
- Advanced polynomial calibration system (1st-3rd degree)
- Hardware watchdog timer for reliability

### Base Station (Display)
- Receives data via ESP-NOW with automatic peer management
- Displays real-time data on TFT screen with modern UI
- Touch interface for navigation and calibration
- Historical graph visualization
- SD card data logging with CSV format
- OG (Original Gravity) stability detection
- ABV (Alcohol By Volume) calculation
- Dark/light theme support
- Comprehensive debug system with conditional compilation

## Hardware Requirements

### Float Unit
- WeMos D32 ESP32 Board with built-in 18650 battery holder
- BMI160 IMU Sensor
- GY-68 BMP180 Temperature/Pressure Sensor
- 18650 Li-ion battery (2000mAh+ recommended)
- SPST Switch (for calibration mode)
- Waterproof enclosure

### Base Station
- 4.0inch ESP32-32E Display (integrated ESP32 + ST7796 display)
- 5V power supply (USB or external)
- Built-in touch screen (resistive)
- MicroSD card (for data logging)

## Installation

### Required Libraries

#### For Float Unit
- **EmotiBit BMI160** (by Connected Future Labs) - Install from GitHub ZIP
  - Download: https://github.com/EmotiBit/EmotiBit_BMI160
  - Install via Sketch → Include Library → Add .ZIP Library
- **Adafruit BMP085 Library** (by Adafruit)
- **Wire** (built-in)
- **EEPROM** (built-in)

#### For Base Station (4.0inch ESP32-32E Display)
- **TFT_eSPI** (by Bodmer)
- **XPT2046_Touchscreen** (by Paul Stoffregen)
- **SD** (built-in)
- **FS** (built-in)
- **SPI** (built-in)

### Installation Steps
1. Install Arduino IDE 2.0+
2. Install required libraries via Library Manager
3. For BMI160: Download ZIP from GitHub and install manually
4. Configure TFT_eSPI User_Setup.h for 4.0inch ESP32-32E display
5. Select appropriate board in Arduino IDE (ESP32 Dev Module)

## Wiring

### Float Unit Connections
```
WeMos D32 ESP32 Board:
- Built-in 18650 battery holder
- Built-in charging circuit (500mA max, USB-C input)
- Built-in power switch
- Built-in battery monitoring (GPIO15 via 51kΩ:51kΩ voltage divider with 10µF capacitor)
- LED_BUILTIN (GPIO5)
- LED_EXTRA (GPIO16) - Calibration mode indicator

BMI160 IMU:
- VCC → 3.3V
- GND → GND
- SDA → GPIO21
- SCL → GPIO22

GY-68 BMP180 Temperature/Pressure Sensor:
- VCC → 3.3V
- GND → GND
- SDA → GPIO21 (shared with BMI160)
- SCL → GPIO22 (shared with BMI160)

Calibration Switch:
- GPIO12 → Switch Terminal 1
- GND → Switch Terminal 2
```

### Base Station Connections
```
4.0inch ESP32-32E Display (Integrated Module):
- No external wiring required
- ESP32, display, and touch screen are integrated
- Power via USB or external 5V supply
- ESP-NOW antenna built-in
- Insert MicroSD card into built-in slot for data logging
```

## Setup and Configuration

### 1. Base Station Setup
1. Insert MicroSD card into the base station's SD card slot
2. Open the `SGNode_Base` folder in Arduino IDE
3. Upload `SGNode_Base.ino` to the base station ESP32
4. On first boot, the system will check for existing fermentation files
5. If existing files are found, you'll be prompted to continue or start new fermentation
6. Use the touch screen to select your choice
7. Open Serial Monitor to get the MAC address
8. Note the MAC address for configuring the float unit

### 2. Float Unit Setup
1. Open the `SGNode_Float` folder in Arduino IDE
2. Update the `baseStationMac` array in `SGNode_Float.ino` with the base station MAC address
3. Upload `SGNode_Float.ino` to the float ESP32
4. Perform calibration (see below)

## Calibration Procedure

### Hardware Switch Calibration (Recommended)
The float unit includes a physical calibration switch for easy calibration via the base station:

**Hardware Setup:**
- Connect SPST switch between GPIO12 and GND on the WeMos D32 float unit
- When switch is activated (closed), float enters calibration mode
- Calibration mode disables deep sleep for continuous operation
- LED_EXTRA (GPIO16) lights up to indicate calibration mode is active

**Calibration Wizard via Base Station:**

1. **Activate Calibration Mode:**
   - Turn on the physical calibration switch on the float unit
   - Float will wake up and stay in calibration mode (high battery drain warning on base station)
   - Base station will show "CALIBRATION MODE ACTIVE" warning

2. **Access Calibration View:**
   - On base station, touch the "CALIB" button
   - Touch "START" to begin the calibration wizard
   - Read the setup instructions, then touch "NEXT"

3. **Step 1/5 - Sensor Offset Calibration:**
   - Place the float unit on a completely flat, level surface
   - Ensure the device is perfectly still (no movement or vibration)
   - Touch the "CALIBRATE" button
   - The float will take 10 accelerometer readings and calculate offset values
   - Offsets are saved to EEPROM and applied to all future readings
   - This ensures the sensor reads 0° when the float is perfectly flat
   - After successful calibration, the system automatically advances to the next step

4. **Prepare Calibration Solutions:**
   - Get a bowl with minimum 3L capacity
   - Fill with 2L tap water (this will be your base solution)
   - Have normal table sugar ready (you'll need 240g total: 80g × 3 additions)

5. **Step-by-Step SG Calibration:**
   
   **Step 2/5 - Water (SG = 1.000):**
   - Place float in the 2L tap water
   - Wait for float to stabilize (no movement)
   - Touch "RECORD" button
   - Float records current tilt angle
   - System automatically advances to next step
   
   **Step 3/5 - Add Sugar (SG = 1.040):**
   - Add 80g normal sugar to the bowl
   - Swirl gently to dissolve completely
   - Wait for float to stabilize
   - Touch "RECORD" button
   - Float records current tilt angle
   - System automatically advances to next step
   
   **Step 4/5 - Add Sugar (SG = 1.080):**
   - Add another 80g normal sugar to the bowl
   - Swirl gently to dissolve completely
   - Wait for float to stabilize
   - Touch "RECORD" button
   - Float records current tilt angle
   - System automatically advances to next step
   
   **Step 5/5 - Add Sugar (SG = 1.120):**
   - Add final 80g normal sugar to the bowl
   - Swirl gently to dissolve completely
   - Wait for float to stabilize
   - Touch "RECORD" button
   - Float records current tilt angle
   - System automatically advances to completion

6. **Apply Calibration:**
   - Review all 4 recorded angles displayed on screen
   - Touch "APPLY" button
   - Float calculates 3rd degree polynomial coefficients using least squares method
   - Coefficients are saved to EEPROM
   - Base station displays confirmation message

7. **Exit Calibration Mode:**
   - Turn off the physical calibration switch on the float unit
   - Float will resume normal deep sleep operation
   - Touch "EXIT" button on base station to return to live view

**Calibration Notes:**
- Sensor offset calibration (Step 1/5) compensates for IMU manufacturing variations
- SG calibration (Steps 2-5) maps tilt angles to specific gravity values
- Both calibrations persist in EEPROM across power cycles
- Perform sensor offset calibration if the float reads non-zero degrees when flat
- Perform SG calibration if density readings are inaccurate
- Recalibrate either system if readings become inaccurate

### Calibration Theory
The system uses a 3rd degree polynomial for accurate calibration:
```
GRAVITY = Coefficient3 × Tilt³ + Coefficient2 × Tilt² + Coefficient1 × Tilt + ConstantTerm
```

Reference points (4 points required):
- Point 1: Water (SG = 1.000)
- Point 2: Light sugar solution (SG ≈ 1.040)
- Point 3: Medium sugar solution (SG ≈ 1.080)
- Point 4: Heavy sugar solution (SG ≈ 1.120)

## Recent Fixes & Improvements (v0.3.0-alpha)

### Critical Fixes
- **✅ Polynomial Calibration Bug** - Fixed matrix back substitution and coefficient mapping
- **✅ Base Station Communication** - Fixed calibration response transmission with rate limiting reset
- **✅ SD Card File Corruption** - Fixed filename validation preventing .c file creation
- **✅ Historical Data Loading** - Added CSV data loading on base station reboot
- **✅ Battery Voltage Calibration** - Updated calibration factor to 1.048 for accurate readings
- **✅ State Machine Completion** - Fixed missing `computeSensorData()` function
- **✅ ESP-NOW Reliability** - Implemented unicast communication with peer management
- **✅ Compilation Issues** - Resolved header multiple definition errors

### Major Enhancements
- **🔧 Calibration System** - Complete 3rd degree polynomial calibration with least squares
- **🔧 Data Persistence** - Historical data loading and file continuation across reboots
- **🔧 Debug System** - Comprehensive conditional compilation debug levels
- **🔧 Peer Management** - Automatic float unit registration and tracking
- **🔧 Error Handling** - Enhanced ESP-NOW error reporting and recovery
- **🔧 File System** - Robust SD card operations with corruption prevention

### Performance & Reliability
- **⚡ Accurate Calibration** - Proper polynomial coefficients for gravity calculations
- **⚡ Data Integrity** - CRC16 verification and file corruption prevention
- **⚡ Battery Accuracy** - Calibrated voltage measurement (4.062V → correct reading)
- **⚡ Reliable Communication** - Unicast instead of broadcast for calibration commands
- **⚡ Better Error Recovery** - Improved sensor initialization and retry logic
- **⚡ Power Optimization** - Enhanced deep sleep and GPIO management

### Confirmed Working Features
- **📊 Peer-to-Peer Communication** - Reliable ESP-NOW data transmission
- **📊 Calibration System** - 4-point calibration with polynomial calculation
- **📊 Data Logging** - CSV file creation and SD card storage
- **📊 Real-time Display** - Live, graph, and detail views with touch interface
- **📊 Battery Monitoring** - Accurate voltage measurement and SOC calculation
- **📊 Temperature Sensing** - BMP180 with EMA filtering
- **📊 Tilt Measurement** - BMI160 3D tilt calculation
- **📊 OG/ABV Calculation** - Automatic original gravity capture and ABV tracking

## System Safeguards & Error Handling

### Communication Reliability
- **ESP-NOW Retry Logic**: Automatic retry for failed transmissions (max 3 attempts)
- **Peer Management**: Automatic float unit registration and MAC address tracking
- **CRC16 Verification**: Data integrity checking on all received packets
- **Channel Forcing**: Ensures both units operate on same ESP-NOW channel

### Sensor & Hardware Protection
- **Sensor Initialization Retry**: Up to 3 attempts for BMI160 and BMP180
- **Hardware Watchdog Timer**: 60-second timeout prevents system hangs
- **Battery Voltage Sanity Checks**: Rejects readings <0.1V or >5.0V
- **Deep Sleep Protection**: Proper WiFi/ESP-NOW deinitialization before sleep

### Calibration System Safeguards
- **Matrix Singularity Detection**: Warns if calibration matrix is ill-conditioned
- **Coefficient Validation**: Checks for NaN/infinity values before saving
- **Tilt Range Normalization**: Normalizes tilt angles to [0,1] for numerical stability
- **EEPROM Version Checking**: Backward compatibility with calibration data

### File System Protection
- **SD Card Write Verification**: Byte count verification for all writes
- **File Extension Validation**: Prevents corrupted .c file creation
- **CSV Format Validation**: Proper parsing with error handling
- **Automatic File Recovery**: Creates new file if corruption detected

### Power Management
- **GPIO Configuration**: All unused pins set to OUTPUT LOW to minimize leakage
- **Deep Sleep Optimization**: Doubles sleep interval when battery <3.3V
- **Battery Monitoring**: Real-time SOC calculation with low-battery warnings
- **Charging Protection**: Built-in over-charge/discharge protection

## Operation

### Float Unit
- Powers on, takes measurements, transmits data, then enters deep sleep
- Measurement interval: 180 seconds (configurable 60-600)
- Automatic power management for extended battery life
- Complete state machine: INIT → MEASURE → COMPUTE → SEND → SLEEP

### Base Station
- Continuous ESP-NOW reception with automatic peer management
- Three display modes:
  - **Live View**: Real-time data display
  - **Graph View**: Historical time-series visualization
  - **Calibration View**: Step-by-step calibration wizard
- Touch buttons to switch between views
- Automatic OG stability detection and ABV calculation

## Display Features

### Live View
- Current specific gravity (SG)
- Original Gravity (OG) - captured automatically when stable
- Alcohol by Volume (ABV) - calculated in real-time
- Tilt angle in degrees
- Temperature in °C
- Battery level with visual indicator
- Signal strength status
- Last update time

### Graph View
- Time-series plot of density changes
- Grid with SG scale (0.9 - 1.2)
- Current value indicator
- Automatic scaling

## Alcohol by Volume (ABV) Calculation

The base station automatically calculates and displays the estimated Alcohol by Volume (ABV) during fermentation.

### Original Gravity (OG) Capture
- The system monitors the first 10 SG readings after fermentation starts
- OG is captured when 3 consecutive readings are stable (variance < 0.001 SG)
- If stability is not achieved by the 10th reading, the average of the last 3 readings is used as OG
- The captured OG is logged to the SD card as a metadata comment

### ABV Calculation
- ABV is calculated using the standard formula: `ABV = (OG - Current SG) × 131.25`
- ABV is updated in real-time as new SG readings arrive
- ABV is displayed on the Live View screen once OG is captured
- ABV values are clamped to 0% to prevent negative values

### Display
- OG is shown in magenta on the Live View screen (only after capture)
- ABV is shown in green on the Live View screen (only after OG capture)
- Both values update automatically as fermentation progresses

## Data Logging

### SD Card Storage
The base station automatically logs all fermentation data to a MicroSD card in CSV format for later analysis.

**CSV Format:**
```
uptime_s,angle,density,temperature,battery_voltage,battery_percent
1234567890,7.1,1.071,20.5,3.85,71
1234568010,7.2,1.070,20.6,3.84,70
...
```

**OG Capture Metadata:**
When the Original Gravity (OG) is captured, it is logged as a comment line in the CSV file:
```
# OG_CAPTURED: 1.048 at reading #5
```

**File Management:**
- Files are stored in `/fermentation/` directory on the SD card
- Each fermentation session creates a sequential file: `fermentation_001.csv`, `fermentation_002.csv`, etc.
- On reboot, the system automatically continues the highest numbered file
- Historical data is loaded into display buffer for immediate viewing
- Data is appended to the selected file automatically
- File corruption prevention with proper `.csv` extension validation

**Historical Data Loading:**
- On power-up, base station scans for existing fermentation files
- Automatically loads up to 100 most recent data points into memory
- Restores Original Gravity (OG) and calibration data from CSV comments
- Live, Graph, and Detail views show previous data immediately
- Seamless continuation of fermentation monitoring across reboots

**SD Card Requirements:**
- FAT32 formatted MicroSD card (2GB-32GB recommended)
- Class 4 or higher for reliable write operations
- Insert card before powering on the base station

**Data Export:**
- Remove SD card and insert into computer to access CSV files
- Files can be opened in Excel, Google Sheets, or any spreadsheet application
- Import data for analysis, graphing, or record keeping

## Troubleshooting

### Common Issues

**No data received**
- Check ESP-NOW MAC address configuration
- Ensure both units are powered
- Verify wiring connections
- Check Serial Monitor for error messages

**Inaccurate readings**
- Perform calibration procedure
- Ensure float is stable in liquid
- Check for mechanical interference
- Verify IMU sensor is functioning

**Battery drains quickly**
- Check deep sleep is working
- Verify measurement interval
- Check for parasitic current draw
- Ensure proper battery voltage

**Touch screen not working**
- Verify XPT2046 connections
- Check touch controller initialization
- Calibrate touch coordinates if needed

**SD card not detected**
- Ensure SD card is properly inserted
- Format SD card as FAT32
- Try a different SD card (Class 4 or higher)
- Check Serial Monitor for "SD Card Failed!" message
- Verify SD_CS pin configuration (default: GPIO13)

**Data not logging to SD card**
- Check that fermentation file is open (Serial Monitor output)
- Verify SD card has sufficient free space
- Ensure file system is not corrupted
- Remove and reinsert SD card, then reboot

### Serial Monitor Debugging

**Float Unit:**
```
=== Fermentation Float Unit Starting ===
Hardware initialized
State: INIT
BMI160 initialized successfully
ESP-NOW initialized
State: MEASURE
Taking 10 samples...Average: ax=0.123, ay=0.045, az=0.987, angle=7.1°
State: COMPUTE
Battery: 3.85V (71%)
State: SEND
Data sent successfully
State: SLEEP
Entering deep sleep...
```

**Base Station:**
```
=== Fermentation Base Station Starting ===
Display initialized
Touch screen initialized
Initializing SD card...
SD card initialized successfully
Created fermentation directory
ESP-NOW initialized
Base station ready
Received: angle=7.10°, density=1.071, temp=20.5°C, battery=71%
Logged data to SD: SG=1.0710, Temp=20.50°C
```

## Technical Specifications

### Performance
- **Measurement accuracy**: ≤0.5° angle resolution, ±0.001 SG accuracy with proper calibration
- **Battery life**: 17+ hours continuous, 2-4 weeks with deep sleep (2000mAh 18650)
- **Transmission range**: ~50m (line of sight), reliable peer-to-peer communication
- **Update rate**: Configurable 60-600 seconds (default 180s)
- **Data storage**: 100 data points on base station + unlimited SD card logging
- **Calibration accuracy**: 3rd degree polynomial with least squares fitting
- **Temperature accuracy**: ±0.1°C with EMA filtering

### Power Consumption
- **Float sleep current**: <0.1mA (deep sleep optimized)
- **Float active current**: <100mA (sensors + transmission)
- **Base station current**: ~290mA (display + SD card)

### Battery Specifications
- **Float unit**: 18650 Li-ion (3.7V nominal, 4.2V max)
- **Voltage calibration**: 1.048 factor (corrected for 13% measurement error)
- **Voltage divider**: 51kΩ:51kΩ with 10µF filtering capacitor
- **SOC calculation**: Linear approximation with low-battery warnings
- **Charging**: Built-in TP4056 circuit (500mA max)

### Power Optimization
The float unit includes several power-saving features:
- All unused GPIOs are configured as OUTPUT LOW to minimize leakage current
- Bluetooth is disabled at startup
- WiFi is completely turned off after each transmission
- Deep sleep interval doubles when battery voltage drops below 3.3V
- Low-power sensor settings (BMI160 at 25Hz, BMP180 in ultralowpower mode)

## File Structure

```
Fermentation/
├── SGNode_Float/
│   ├── SGNode_Float.ino           # Float unit firmware
│   └── polynomial_calibration.h   # Calibration functions
├── SGNode_Base/
│   ├── SGNode_Base.ino            # Base station firmware
│   ├── logo.h                     # Boot logo image data (RGB565)
│   └── polynomial_calibration.h   # Calibration functions
├── README.md                      # This documentation
├── hardware.md                    # Hardware specifications and BOM
├── WIRING_DIAGRAM.md              # Detailed wiring instructions
└── CHANGELOG.md                   # Project changelog
```

## License

This project is open source. Feel free to modify and adapt for your specific fermentation monitoring needs.

## Support

For technical support or questions, refer to the troubleshooting section or check the Serial Monitor output for diagnostic information.
