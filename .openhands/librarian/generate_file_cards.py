#!/usr/bin/env python3
"""Generate file cards for key SGNode source files."""
import os
import hashlib
import json

def calculate_hash(filepath):
    """Calculate SHA256 hash of a file."""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def generate_file_card(filepath, project_root):
    """Generate markdown content for a file card."""
    rel_path = os.path.relpath(filepath, project_root)
    file_hash = calculate_hash(filepath)
    
    # Determine language and type
    if rel_path.endswith('.ino'):
        language = 'Arduino/C++'
    elif rel_path.endswith('.h'):
        language = 'C++ Header'
    elif rel_path.endswith('.cpp'):
        language = 'C++ Source'
    else:
        language = 'Other'
    
    # Generate card content
    lines = []
    lines.append(f"# {rel_path}")
    lines.append("")
    lines.append(f"**Type:** {language}")
    lines.append(f"**Hash:** `{file_hash}`")
    lines.append("")
    
    # Read and analyze file
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read(3000)  # First 3000 chars for analysis
    except:
        content = ""
    
    # Extract key information based on file type
    if rel_path.endswith('.h'):
        # Header file analysis
        if 'sg_protocol' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("ESP-NOW protocol definitions for Base↔Float and Base↔Plug communication.")
            lines.append("")
            lines.append("## Key Structures")
            lines.append("")
            lines.append("- `payload_t` - Float data packet (26 bytes)")
            lines.append("- `sg_plug_command_t` - Plug control command (107 bytes)")
            lines.append("- `sg_plug_status_t` - Plug status report (61 bytes)")
            lines.append("- `ack_packet_t` - ACK with command (7 bytes)")
            lines.append("")
            lines.append("## Protocol Features")
            lines.append("")
            lines.append("- Version 2 protocol")
            lines.append("- CRC16-CCITT checksums")
            lines.append("- Sequence number tracking")
            lines.append("- Zero-calibration commands via ACK")
        elif 'plug_hardware' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Plug hardware pin definitions and helper functions.")
            lines.append("")
            lines.append("## Pin Assignments")
            lines.append("")
            lines.append("- LED Primary: GPIO13")
            lines.append("- LED Secondary: GPIO1")
            lines.append("- Button: GPIO3")
            lines.append("- Air Sensor: GPIO4 (DS18B20)")
            lines.append("- Beer Sensor: GPIO5 (DS18B20)")
            lines.append("- Relay: GPIO14 (active high)")
            lines.append("")
            lines.append("## Functions")
            lines.append("")
            lines.append("- `beginRelayOff()` - Initialize relay off")
            lines.append("- `setRelay(on)` - Control relay state")
        else:
            lines.append("## Role")
            lines.append("")
            lines.append("Header file containing declarations.")
            lines.append("")
            lines.append("## Key Elements")
            lines.append("")
            lines.append("- See source file for details")
    
    elif rel_path.endswith('.cpp'):
        # Source file analysis
        if 'yeast_preset' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Yeast preset management and repository.")
            lines.append("")
            lines.append("## Key Classes")
            lines.append("")
            lines.append("- `YeastPreset` - Yeast configuration (name, temp range, attenuation, etc.)")
            lines.append("- `YeastPresetRepository` - Load/save/custom yeast management")
            lines.append("")
            lines.append("## Preset Types")
            lines.append("")
            lines.append("- Clean ale (US-05, Nottingham)")
            lines.append("- Fast ale (S-04)")
            lines.append("- Slow clean ale (K-97/Kölsch)")
            lines.append("- Lager (W-34/70, S-23)")
            lines.append("- Very fast ale (Voss Kveik)")
            lines.append("- High attenuation saison (Belle Saison)")
        elif 'batch_action' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Batch actions (dry hop, cold crash, D-rest, package).")
            lines.append("")
            lines.append("## Action Types")
            lines.append("")
            lines.append("- `DRY_HOP` - Dry hop addition")
            lines.append("- `COLD_CRASH` - Temperature crash")
            lines.append("- `DIACETYL_REST` - Diacetyl rest")
            lines.append("- `PACKAGE` - Bottling/kegging")
            lines.append("")
            lines.append("## Features")
            lines.append("")
            lines.append("- Action state tracking")
            lines.append("- Required action flags")
        elif 'fermentation_state' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Fermentation state machine and phase tracking.")
            lines.append("")
            lines.append("## State Machine")
            lines.append("")
            lines.append("- Pitching")
            lines.append("- Primary fermentation")
            lines.append("- D-rest (if needed)")
            lines.append("- Cold crash")
            lines.append("- Package")
            lines.append("- Storage/Lagering")
            lines.append("")
            lines.append("## State Transitions")
            lines.append("")
            lines.append("- Driven by SG trend, attenuation, and batch actions")
            lines.append("- Phase holds require temperature stability")
        elif 'brix_converter' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Brix to SG conversion using polynomial calibration.")
            lines.append("")
            lines.append("## Key Functions")
            lines.append("")
            lines.append("- `brix_to_sg()` - Convert Brix to SG")
            lines.append("- `sg_to_brix()` - Convert SG to Brix")
            lines.append("- Uses EEPROM-stored polynomial coefficients from float")
        elif 'derived_calculations' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Derived metrics: attenuation, ABV, SG trend.")
            lines.append("")
            lines.append("## Calculations")
            lines.append("")
            lines.append("- `calculate_attenuation()` - FG relative to OG")
            lines.append("- `calculate_abv()` - Alcohol by volume")
            lines.append("- `calculate_sg_trend()` - Rate of change")
            lines.append("- `calculate_volume_remaining()` - Estimated remaining volume")
        elif 'target_curve' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Temperature target curve generation and management.")
            lines.append("")
            lines.append("## Curve Types")
            lines.append("")
            lines.append("- `Lager` - Lower temps, longer times")
            lines.append("- `Clean Ale` - Moderate temps")
            lines.append("- `Fast Ale` - Shorter holds")
            lines.append("- `Saison` - Warm, rising profile")
            lines.append("")
            lines.append("## Phase Structure")
            lines.append("")
            lines.append("- Pitch (yeast addition)")
            lines.append("- Main fermentation")
            lines.append("- D-rest (diacetyl rest)")
            lines.append("- Cold crash")
            lines.append("- Package/storage")
        elif 'beer_pi_controller' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("PI controller with D-brake and anti-windup for temperature control.")
            lines.append("")
            lines.append("## Controller Features")
            lines.append("")
            lines.append("- PI control with configurable Kp, Tn")
            lines.append("- D-brake for overshoot reduction")
            lines.append("- Anti-windup for integral term")
            lines.append("- Cold/warm integral bands")
            lines.append("- Warming/cooling mode detection")
        elif 'air_controller' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Fridge air temperature target calculation from beer target.")
            lines.append("")
            lines.append("## Control Logic")
            lines.append("")
            lines.append("- Beer probe as primary feedback")
            lines.append("- Air target derived from PI output")
            lines.append("- Min/max air target clamping")
            lines.append("- Compressor protection timing")
        elif 'plug_transport' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("ESP-NOW transport for Plug communication with base.")
            lines.append("")
            lines.append("## Features")
            lines.append("")
            lines.append("- Receive commands from base")
            lines.append("- Send status reports")
            lines.append("- Command timeout detection")
            lines.append("- CRC validation")
        elif 'plug_sensors' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("DS18B20 temperature sensor reading.")
            lines.append("")
            lines.append("## Sensors")
            lines.append("")
            lines.append("- Air sensor (fridge air temperature)")
            lines.append("- Beer sensor (fermenting beer temperature)")
            lines.append("")
            lines.append("## Features")
            lines.append("")
            lines.append("- OneWire bus communication")
            lines.append("- CRC validation")
            lines.append("- Validity flags")
        elif 'relay_history' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Relay duty cycle tracking and compressor pattern.")
            lines.append("")
            lines.append("## Features")
            lines.append("")
            lines.append("- 10-minute duty window")
            lines.append("- Compressor on/off pattern generator")
            lines.append("- Minimum on/off time enforcement")
        elif 'brew_wizard' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Interactive batch creation wizard.")
            lines.append("")
            lines.append("## Steps")
            lines.append("")
            lines.append("- Batch name and style")
            lines.append("- Volume and initial Brix")
            lines.append("- Yeast selection")
            lines.append("- Temperature profile generation")
            lines.append("- Preview and confirm")
        elif 'ui_components' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Shared UI components for base station touchscreen.")
            lines.append("")
            lines.append("## Components")
            lines.append("")
            lines.append("- Chart rendering (SG, temp, angle, battery)")
            lines.append("- Button and menu system")
            lines.append("- Theme support (light/dark)")
            lines.append("- Touch input handling")
        elif 'eta_predictor' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("ETA calculation based on fermentation rate.")
            lines.append("")
            lines.append("## Method")
            lines.append("")
            lines.append("- Current attenuation rate")
            lines.append("- Target final gravity")
            lines.append("- Estimated time to reach FG")
        elif 'og_verifier' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("OG stability detection and verification.")
            lines.append("")
            lines.append("## Method")
            lines.append("")
            lines.append("- Monitor SG over time")
            lines.append("- Detect stable readings")
            lines.append("- Allow user to confirm or override")
        elif 'recommendation_engine' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Generate actionable recommendations based on fermentation state.")
            lines.append("")
            lines.append("## Recommendation Types")
            lines.append("")
            lines.append("- Action needed (dry hop, D-rest, crash)")
            lines.append("- Informational (FG stable, ready to package)")
            lines.append("- Warning (temperature out of range)")
        else:
            lines.append("## Role")
            lines.append("")
            lines.append("Core component implementation.")
            lines.append("")
            lines.append("## Key Features")
            lines.append("")
            lines.append("- See source code for details")
    
    elif rel_path.endswith('.ino'):
        # Main sketch file
        if 'SGNode_Base' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Base station main entry point and UI orchestration.")
            lines.append("")
            lines.append("## Main Responsibilities")
            lines.append("")
            lines.append("- ESP-NOW receiver for float packets")
            lines.append("- Touchscreen UI management")
            lines.append("- SD card data logging")
            lines.append("- Batch and yeast management")
            lines.append("- Chart and graph rendering")
            lines.append("")
            lines.append("## Key Functions")
            lines.append("")
            lines.append("- `handlePacketReceived()` - Process float data")
            lines.append("- `handleACK()` - Send commands to float")
            lines.append("- `updateDisplay()` - Refresh UI")
            lines.append("- `saveBatchData()` - Write to SD card")
            lines.append("")
            lines.append("## Debug Control")
            lines.append("")
            lines.append("- `SGNODE_DEBUG_LEVEL` - Quiet/Error/Info/Verbose")
            lines.append("- `SGNODE_UI_TEST_HARNESS` - Enable test mode")
        elif 'SGNode_Float' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Float unit main entry point and sensor management.")
            lines.append("")
            lines.append("## Main Responsibilities")
            lines.append("")
            lines.append("- BMI160 IMU measurement")
            lines.append("- Polynomial SG calibration")
            lines.append("- ESP-NOW transmission")
            lines.append("- Deep sleep power management")
            lines.append("- Battery voltage monitoring")
            lines.append("")
            lines.append("## Key Functions")
            lines.append("")
            lines.append("- `measureAndTransmit()` - Take reading and send")
            lines.append("- `zeroCalibrate()` - Accelerometer zero calibration")
            lines.append("- `enterDeepSleep()` - Power down between cycles")
            lines.append("- `handleACK()` - Process base station commands")
            lines.append("")
            lines.append("## Configuration")
            lines.append("")
            lines.append("- `MEASUREMENT_INTERVAL` - Seconds between readings (60-600)")
            lines.append("- `baseStationMac` - Base station MAC address")
        elif 'SGNode_Plug' in rel_path:
            lines.append("## Role")
            lines.append("")
            lines.append("Plug temperature controller main entry point.")
            lines.append("")
            lines.append("## Main Responsibilities")
            lines.append("")
            lines.append("- DS18B20 temperature sensing")
            lines.append("- PI control with D-brake")
            lines.append("- ESP-NOW communication with base")
            lines.append("- Relay control (fridge/compressor)")
            lines.append("- Fault detection and reporting")
            lines.append("")
            lines.append("## Key Functions")
            lines.append("")
            lines.append("- `updateFaults()` - Sensor and command validation")
            lines.append("- `applyCommandState()` - Process base commands")
            lines.append("- `setActualRelay()` - Control compressor")
            lines.append("- `printCommandIfChanged()` - Log command updates")
            lines.append("")
            lines.append("## Control Modes")
            lines.append("")
            lines.append("- `OFF` - No control")
            lines.append("- `AUTO` - PI control with beer probe")
            lines.append("- `AIR_ONLY` - Post-package air-only mode")
    
    # Add dependencies section
    lines.append("")
    lines.append("## Key Dependencies")
    lines.append("")
    lines.append("- See file header for required libraries")
    
    # Add side effects
    lines.append("")
    lines.append("## Side Effects")
    lines.append("")
    if 'relay' in rel_path.lower() or 'plug' in rel_path.lower():
        lines.append("- Controls physical relay (fridge compressor)")
        lines.append("- Affects temperature regulation")
    elif 'sd' in rel_path.lower() or 'batch' in rel_path.lower() or 'log' in rel_path.lower():
        lines.append("- Writes to SD card (CSV logs)")
        lines.append("- Updates batch JSON files")
    elif 'eeprom' in rel_path.lower() or 'calibrat' in rel_path.lower():
        lines.append("- Writes to float EEPROM (calibration coefficients)")
        lines.append("- Persists across power cycles")
    else:
        lines.append("- Memory allocation (RAM)")
        lines.append("- Serial debug output (if enabled)")
    
    # Add test info
    lines.append("")
    lines.append("## Tests")
    lines.append("")
    lines.append("- No automated tests")
    lines.append("- Manual testing required")
    
    # Add risk info
    lines.append("")
    lines.append("## Change Risks")
    lines.append("")
    if 'relay' in rel_path.lower() or 'plug' in rel_path.lower():
        lines.append("- **HIGH**: Relay control affects physical hardware")
        lines.append("- Incorrect PI parameters can damage compressor")
    elif 'calibrat' in rel_path.lower() or 'polynomial' in rel_path.lower():
        lines.append("- **HIGH**: Affects all SG calculations")
        lines.append("- Invalid coefficients corrupt all data")
    elif 'batch' in rel_path.lower() or 'log' in rel_path.lower():
        lines.append("- **MEDIUM**: Data persistence")
        lines.append("- Corrupt JSON can lose batch history")
    else:
        lines.append("- **MEDIUM**: Core functionality")
        lines.append("- Test thoroughly before deployment")
    
    lines.append("")
    lines.append("---")
    lines.append("")
    lines.append(f"*File last modified (hash): {file_hash}*")
    
    return "\n".join(lines)

def main():
    project_root = '/tmp/conversation-worktrees/106e6d18-003a-48ea-b9d7-5283b591d2ab/SGNode'
    output_dir = os.path.join(project_root, '.openhands/librarian/files')
    
    # Key files to generate cards for (priority order)
    key_files = [
        'SGNode_Shared/sg_protocol.h',
        'SGNode_Shared/plug_hardware.h',
        'SGNode_Base/SGNode_Base.ino',
        'SGNode_Float/SGNode_Float.ino',
        'SGNode_Plug/SGNode_Plug.ino',
        'SGNode_Base/src/domain/yeast_preset_repository.cpp',
        'SGNode_Base/src/domain/yeast_preset_repository.h',
        'SGNode_Base/src/domain/batch_action.cpp',
        'SGNode_Base/src/domain/batch_action.h',
        'SGNode_Base/src/domain/fermentation_state_machine.cpp',
        'SGNode_Base/src/domain/fermentation_state_machine.h',
        'SGNode_Base/src/domain/recommendation_engine.cpp',
        'SGNode_Base/src/domain/recommendation_engine.h',
        'SGNode_Base/src/domain/brew_profile.cpp',
        'SGNode_Base/src/domain/brew_profile.h',
        'SGNode_Base/src/domain/yeast_preset.cpp',
        'SGNode_Base/src/domain/yeast_preset.h',
        'SGNode_Base/src/calculations/brix_converter.cpp',
        'SGNode_Base/src/calculations/brix_converter.h',
        'SGNode_Base/src/calculations/derived_calculations.cpp',
        'SGNode_Base/src/calculations/derived_calculations.h',
        'SGNode_Base/src/calculations/eta_predictor.cpp',
        'SGNode_Base/src/calculations/eta_predictor.h',
        'SGNode_Base/src/calculations/og_verifier.cpp',
        'SGNode_Base/src/calculations/og_verifier.h',
        'SGNode_Base/src/calculations/target_curve.cpp',
        'SGNode_Base/src/calculations/target_curve.h',
        'SGNode_Base/src/ui/brew_wizard_controller.cpp',
        'SGNode_Base/src/ui/brew_wizard_controller.h',
        'SGNode_Base/src/ui/ui_components.cpp',
        'SGNode_Base/src/ui/ui_components.h',
        'SGNode_Base/src/test/ui_test_harness.cpp',
        'SGNode_Base/src/test/ui_test_harness.h',
        'SGNode_Plug/src/plug_sensors.cpp',
        'SGNode_Plug/src/plug_sensors.h',
        'SGNode_Plug/src/plug_transport.cpp',
        'SGNode_Plug/src/plug_transport.h',
        'SGNode_Plug/src/plug_types.h',
        'SGNode_Plug/src/plug_selfcheck.cpp',
        'SGNode_Plug/src/plug_selfcheck.h',
        'SGNode_Plug/src/beer_pi_controller.cpp',
        'SGNode_Plug/src/beer_pi_controller.h',
        'SGNode_Plug/src/air_controller.cpp',
        'SGNode_Plug/src/air_controller.h',
        'SGNode_Plug/src/relay_history.cpp',
        'SGNode_Plug/src/relay_history.h',
    ]
    
    # Filter to existing files
    existing_files = []
    for f in key_files:
        full_path = os.path.join(project_root, f)
        if os.path.exists(full_path):
            existing_files.append(f)
    
    print(f"Generating {len(existing_files)} file cards...")
    
    # Track generated names to avoid duplicates
    generated_names = set()
    
    for rel_path in existing_files:
        full_path = os.path.join(project_root, rel_path)
        card_content = generate_file_card(full_path, project_root)
        
        # Generate safe filename with unique suffix for duplicates
        base_name = rel_path.replace('/', '_').replace('.h', '').replace('.cpp', '').replace('.ino', '')
        safe_name = base_name + '.md'
        
        # Handle duplicates by adding path-based disambiguation
        if safe_name in generated_names:
            # Create unique name based on directory
            dir_name = os.path.dirname(rel_path).replace('/', '_')
            safe_name = base_name + '_' + dir_name + '.md'
        
        generated_names.add(safe_name)
        output_path = os.path.join(output_dir, safe_name)
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(card_content)
        
        print(f"  Generated: {safe_name}")
    
    print(f"\nGenerated {len(existing_files)} file cards to {output_dir}")

if __name__ == '__main__':
    main()
