# Changelog

All notable changes to SGNode will be documented in this file.

## [0.2.0-beta] - 2026-04-28

### Critical Fixes
- **Fixed:** Missing `computeSensorData()` function causing state machine failure
- **Fixed:** ESP-NOW broadcast communication - now uses reliable unicast with peer management
- **Fixed:** Header multiple definition errors - converted globals to extern declarations
- **Fixed:** Base station MAC address displaying as zeros - added WiFi initialization delay
- **Fixed:** Calibration header design preventing project expansion
- **Fixed:** Function prototype mismatches and missing implementations

### Major Enhancements
- **Added:** Comprehensive debug system with conditional compilation (DEBUG_NONE/ERROR/INFO/VERBOSE)
- **Added:** Automatic ESP-NOW peer registration and MAC address tracking
- **Added:** Enhanced error handling and recovery for ESP-NOW operations
- **Added:** Complete state machine implementation (INIT/MEASURE/COMPUTE/SEND/SLEEP)
- **Added:** Hardware watchdog timer for system reliability

### Code Quality
- **Improved:** Project structure - archived unnecessary files (~1.2MB saved)
- **Improved:** Memory management with proper header/definition separation
- **Improved:** Error reporting with detailed debug output
- **Improved:** Calibration command reliability with unicast transmission

### Performance
- **Enhanced:** Sensor initialization with retry logic and error detection
- **Enhanced:** Battery voltage monitoring with proper ADC configuration
- **Enhanced:** Temperature filtering with EMA (Exponential Moving Average)
- **Enhanced:** Deep sleep power management

### Documentation
- **Updated:** README.md with current features and installation instructions
- **Updated:** Library requirements with specific BMI160 installation steps
- **Added:** Recent fixes and improvements section

## [0.1.0-alpha] - 2026-04-20

### Fixed
- **Critical:** Added `__attribute__((packed))` to all payload structures to prevent memory alignment issues
- **Critical:** Added ESP-NOW channel forcing to prevent channel mismatch
- **Critical:** Added ADC attenuation configuration (ADC_11db) for accurate battery voltage readings
- **Critical:** Removed GPIO 6-11 from unused GPIO configuration (FLASH pins)
- **Critical:** Fixed calibration switch pinMode order (set to INPUT_PULLUP before reading)
- **Critical:** Added division by zero protection in graph scaling calculations
- **Critical:** Replaced `abs()` with `fabs()` for float operations
- **Medium:** Added `initCalibration()` call in setup() to load EEPROM calibration at boot
- **Medium:** Added WiFi/ESP-NOW deinitialization before deep sleep to prevent power drain
- **Medium:** Added SD card write error handling with byte count verification
- **Medium:** Added EEPROM commit success checking for calibration saves
- **Medium:** Added sensor initialization retry with 1-second delays (BMI160, BMP180)
- **Low:** Added I2C bus error checking with device detection validation
- **Low:** Added watchdog reset in long polynomial calculations to prevent timeout
- **Low:** Added Serial.flush() before deep sleep to ensure all debug output is sent

### Changed
- **High:** Improved tilt formula from `atan2(ax, az)` to `atan2(ax, sqrt(ay*ay + az*az))` for accurate 3D tilt measurement
- **High:** Enhanced polynomial calibration with:
  - Pivot zero-detection for matrix singularity warnings
  - Tilt input normalization to [0,1] range for numerical stability
  - Adaptive polynomial degree: linear (2 pts), quadratic (3 pts), cubic (4+ pts)
  - Normalization parameters stored in EEPROM
- **High:** Added protocol version field (version 2) to payload_t for future compatibility
- **High:** Added SD card flush() after all writes to prevent data loss
- **High:** Added EEPROM version checking with backward compatibility (version 1 support)
- **High:** Added coefficient validation (NaN/infinity checks) before saving calibration
- Updated CRC calculation to exclude version field for backward compatibility
- Increased EEPROM reservation to 64 bytes for additional calibration parameters

## [0.1.0-alpha] - 2026-04-25

### Added
- Initial release of SGNode fermentation monitoring system
- Float unit with BMI160 IMU and BMP180 temperature sensor
- Base station with TFT display and touch interface
- ESP-NOW wireless communication
- Deep sleep power management for extended battery life
- 4-point calibration wizard with 3rd degree polynomial
- Original Gravity (OG) automatic capture with stability detection
- Alcohol by Volume (ABV) real-time calculation
- SD card data logging with CSV format
- CRC16 data integrity verification
- Battery voltage monitoring and percentage display
- Power optimization (unused GPIOs configured as OUTPUT LOW)
- Hardware switch for calibration mode activation

### Fixed
- Critical payload_t struct mismatch between float and base station
- GPIO array size calculation in float unit
- Documentation inconsistencies

### Changed
- Rebranded to SGNode
- Reorganized file structure for Arduino IDE compatibility
- Updated CSV format to include battery voltage and percentage
