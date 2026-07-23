# SGNode Commands Reference

## Build Commands

**Note:** SGNode uses Arduino IDE for compilation. No CLI build system available.

| Task | Command/Steps | Notes |
|------|--------------|-------|
| **Build Base** | Arduino IDE → Sketch → Verify/Compile `SGNode_Base.ino` | Select ESP32 board |
| **Build Float** | Arduino IDE → Sketch → Verify/Compile `SGNode_Float.ino` | Select ESP32 board |
| **Build Plug** | Arduino IDE → Sketch → Verify/Compile `SGNode_Plug.ino` | Select ESP8266 board |
| **Flash Base** | Arduino IDE → Sketch → Upload | USB connection required |
| **Flash Float** | Arduino IDE → Sketch → Upload | USB connection required |
| **Flash Plug** | Arduino IDE → Sketch → Upload | USB connection required |

### Build Prerequisites
1. Arduino IDE 2.x
2. ESP32 board support package
3. ESP8266 board support package
4. Libraries: `TFT_eSPI`, `XPT2046_Touchscreen`, `EmotiBit BMI160`, `OneWire`, `DallasTemperature`

## Test Commands

**Note:** No automated test system. Manual testing only.

| Task | Method | Notes |
|------|--------|-------|
| **Unit Tests** | None | No testing framework |
| **Integration Tests** | Manual testing with hardware | Use test harness in `SGNode_Base/src/test/` |
| **Serial Debug** | `#define SGNODE_DEBUG_LEVEL DEBUG_INFO` in `SGNode_Base.ino` | Levels: QUIET, ERROR, INFO, VERBOSE |

## Lint/Format Commands

**Note:** No linting or formatting tools available.

| Task | Tool/Method | Notes |
|------|-------------|-------|
| **Lint** | None | Arduino IDE compilation catches syntax errors |
| **Format** | None | Manual formatting |

## Development Start

| Task | Command/Steps | Notes |
|------|--------------|-------|
| **Start Development** | Open `SGNode_Base/SGNode_Base.ino` in Arduino IDE | Base is primary development focus |
| **Enable Debug** | Set `SGNODE_DEBUG_LEVEL` in main sketch | Use `DEBUG_INFO` for setup, `DEBUG_VERBOSE` for diagnostics |

## Serial Commands

### Base Station Debug Output
```cpp
// In SGNode_Base.ino, set debug level:
#define SGNODE_DEBUG_LEVEL DEBUG_INFO  // or QUIET, ERROR, VERBOSE
```

### Float Test Harness
```cpp
// In SGNode_Float.ino, enable test mode:
#define SGNODE_FLOAT_TEST_HARNESS 1  // Mock sensors, pause state machine
```

## Hardware Interaction

| Task | Method | Notes |
|------|--------|-------|
| **Zero Calibration** | Dashboard → Level Float → READY → Put Float In Brew → DONE | Must confirm before logging |
| **SG Calibration** | More → Calibration → Record points → Apply polynomial | Stored in float EEPROM |
| **SD Card** | FAT32, inserted before boot | Paths: `/data/batches/`, `/data/yeast/` |
| **RTC** | Optional DS323x on GPIO32/25 | Improves timestamp stability |

## Debug Output

**Base Station:** Serial at 115200 baud

**Debug Levels:**
- `DEBUG_QUIET` - No serial output
- `DEBUG_ERROR` - Only errors
- `DEBUG_INFO` - Setup details (use for MAC address)
- `DEBUG_VERBOSE` - Packet/SD/graph diagnostics

**Use Cases:**
- Setup: `DEBUG_INFO` for MAC address, ESP-NOW pairing
- Troubleshooting: `DEBUG_VERBOSE` for packet loss, SD issues
