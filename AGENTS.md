# SGNode Agent Memory

This is an ESP32-based fermentation monitoring system for brewing (beer/wine/cider/mead).

## Quick Overview

**What it does:** Monitors fermentation via a battery-powered float (tilt/temperature) and optional temperature control plug. Data displayed on touchscreen base station. No cloud/phone app needed.

**Architecture:**
- **SGNode_Base**: ESP32 base station with touchscreen, receives float data via ESP-NOW, stores on SD
- **SGNode_Float**: Battery-powered sensor float with BMI160 IMU, deep-sleep enabled
- **SGNode_Plug**: ESP8285 smart plug with PI temperature control, connects to base via ESP-NOW

**Language:** C++ (Arduino framework) for all three components

## File Structure

```
SGNode_Base/      - Base station firmware (main: SGNode_Base.ino)
SGNode_Float/     - Float unit firmware (main: SGNode_Float.ino)
SGNode_Plug/      - Temperature control plug (main: SGNode_Plug.ino)
SGNode_Shared/    - Shared headers: sg_protocol.h, plug_hardware.h
docs/             - Technical documentation
```

## Key Commands

- **Build:** Use Arduino IDE 2.x (no CLI commands; flash via USB)
- **Test:** Manual testing only (no automated tests)
- **Lint:** None (Arduino IDE handles compilation)
- **Start:** Upload firmware to ESP32/ESP8266 devices

## Librarian System

### When to Update

**After any code changes** (new files, modified headers, updated functions/structs), the librarian must be updated to keep the documentation synchronized with the source code.

### How to Update

```bash
python3 .openhands/librarian/update_librarian.py
```

This will:
- Detect changed files via git
- Update the file map with new/modified entries
- Regenerate file cards for changed files with full API details

For a full regeneration (not incremental):

```bash
python3 .openhands/librarian/generate_file_cards_improved.py
```

### What Gets Updated

- **file-map.json**: Updated with new file entries and hashes
- **files/*.md**: Detailed API cards for each source file including:
  - Structs/classes with all fields and methods
  - Function signatures with parameters and return types
  - Enums with all values
  - Dependencies (local and system includes)
  - Side effects and hardware access
  - Change risk assessment

## Important Conventions

- **Code is truth:** Librarian memory supplements, never replaces, reading source code
- **Protocol:** ESP-NOW for wireless communication, CRC16 checksums
- **Persistence:** SD card FAT32, JSON for batches/yeast, CSV for logs
- **Calibration:** Polynomial SG calibration in float EEPROM, zero-cal via ACK packets
- **No build system:** Arduino IDE manual compilation/upload

## Reference

For full details, see `.openhands/librarian/index.md` and `.openhands/librarian/commands.md`.
