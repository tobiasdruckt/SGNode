# Changelog

All notable changes to SGNode will be documented in this file.

## [Unreleased]

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
