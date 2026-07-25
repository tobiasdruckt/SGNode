# SGNode_Plug/SGNode_Plug.ino

**Type:** Arduino Sketch
**Hash:** `210d7b82468cb15e6b0205495257e4a68aac291dbe3e847a0a39edf12e2b821d`

## Overview / Role

Core functionality module

## Verified API & Symbols

### Functions

- `void clampPlugValue (float value, float minimum, float maximum)`

- `void updateFaults (const SensorSnapshot& sample) const`

- `void statusFaultsForCurrentMode (void)`

- `void applyCommandState (uint32_t nowMs)`

- `void sendPlugStatus (uint32_t nowMs)`

- `void setActualRelay (bool on)`

- `void updateFallbackMode (void)`

- `void printStatus (void)`

- `void printCommandIfChanged (void)`

- `void updateOtaButton (uint32_t nowMs)`

- `void otaDiagnosticText (void)`

- `void handleOtaRoot (void)`

- `void handleOtaDiag (void)`

- `void handleOtaLogo (void)`

- `void handleOtaReboot (void)`

- `void handleOtaUpdateDone (void)`

- `void handleOtaUpload (void)`

- `void startOtaMode (uint32_t nowMs)`

- `void handleOtaMode (uint32_t nowMs)`

- `void setup (void)`

- `void loop (void)`

## Key Dependencies

**Local:**
- `SGNode_Plug/../SGNode_Shared/sg_logo_png.h`
- `SGNode_Plug/src/air_controller.h`
- `SGNode_Plug/src/beer_pi_controller.h`
- `SGNode_Plug/src/plug_sensors.h`
- `SGNode_Plug/src/plug_selfcheck.h`
- `SGNode_Plug/src/plug_transport.h`
- `SGNode_Plug/src/plug_types.h`
- `SGNode_Plug/src/relay_history.h`

**System/External:**
- `<ESP8266WebServer.h>`
- `<ESP8266WiFi.h>`
- `<Updater.h>`
- `<espnow.h>`
- `<plug_hardware.h>`

## Side Effects & Hardware Access

- ESP-NOW packet transmission
- Relay control (fridge compressor)
- Sensor reading
- Temperature target calculation
- WiFi mode configuration
- WiFi access point mode
- Serial port initialization
- Serial output

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): 210d7b82468cb15e6b0205495257e4a68aac291dbe3e847a0a39edf12e2b821d*