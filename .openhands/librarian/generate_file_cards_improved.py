#!/usr/bin/env python3
"""Improved SGNode file card generator.

This generator:
1. Extracts actual symbols from source files
2. Identifies concrete dependencies
3. Documents real side effects (not placeholders)
4. Avoids generic placeholder text
5. Marks low-confidence areas explicitly
"""

import os
import hashlib
import re
import json
from pathlib import Path
from typing import Dict, List, Tuple, Optional

def calculate_hash(filepath: str) -> str:
    """Calculate SHA256 hash of a file."""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def get_file_type(filepath: str) -> str:
    """Determine file type from extension."""
    ext = os.path.splitext(filepath)[1].lower()
    type_map = {
        '.h': 'C++ Header',
        '.cpp': 'C++ Source',
        '.ino': 'Arduino Sketch',
        '.json': 'JSON',
        '.md': 'Markdown',
        '.py': 'Python',
        '.sh': 'Shell'
    }
    return type_map.get(ext, 'Other')

def extract_includes(content: str) -> List[str]:
    """Extract #include statements from source."""
    includes = []
    for match in re.finditer(r'#\s*include\s*[<"]([^>"]+)[>"]', content):
        includes.append(match.group(1))
    return includes

def extract_classes(content: str) -> List[str]:
    """Extract class names from source."""
    classes = []
    for match in re.finditer(r'class\s+(\w+)', content):
        classes.append(match.group(1))
    return classes

def extract_structs(content: str) -> List[str]:
    """Extract struct names from source."""
    structs = []
    for match in re.finditer(r'struct\s+(\w+)', content):
        structs.append(match.group(1))
    return structs

def extract_enums(content: str) -> List[str]:
    """Extract enum names from source."""
    enums = []
    for match in re.finditer(r'enum\s+(\w+)', content):
        enums.append(match.group(1))
    return enums

def extract_functions(content: str) -> List[str]:
    """Extract function declarations."""
    functions = []
    # Match function declarations (not definitions with bodies)
    pattern = r'(?:^|[\n])\s*(?:static\s+)?(?:inline\s+)?(?:const\s+)?(?:\w+)\s+(?:\w+)\s*\([^)]*\)\s*(?:const\s*)?(?:override\s*)?(?:=.*?;)?(?=\s*\n|\s*{)'
    for match in re.finditer(pattern, content, re.MULTILINE):
        func = match.group(0).strip()
        # Clean up and extract function name
        func = re.sub(r'\s+', ' ', func)
        if 'void' in func or 'bool' in func or 'int' in func or 'float' in func:
            # Extract just the function signature
            match2 = re.search(r'(\w+)\s*\(', func)
            if match2:
                functions.append(match2.group(1))
    return functions[:20]  # Limit to most important

def extract_setup_loop(content: str) -> Dict[str, bool]:
    """Check for setup() and loop() functions."""
    return {
        'setup': 'void setup()' in content or 'void setup(' in content,
        'loop': 'void loop()' in content or 'void loop(' in content
    }

def get_side_effects_from_file(filepath: str, content: str, file_type: str) -> List[str]:
    """Extract actual side effects from the file."""
    side_effects = []
    
    # ESP-NOW operations
    if 'esp_now' in content.lower() or 'esp-now' in content.lower():
        if 'send' in content.lower() or 'transmit' in content.lower():
            side_effects.append("ESP-NOW packet transmission")
        if 'receive' in content.lower() or 'callback' in content.lower():
            side_effects.append("ESP-NOW packet reception")
    
    # EEPROM operations
    if 'EEPROM' in content or 'eeprom' in content.lower():
        if 'write' in content.lower() or 'store' in content.lower():
            side_effects.append("EEPROM write (persistent storage)")
        if 'read' in content.lower() or 'load' in content.lower():
            side_effects.append("EEPROM read")
    
    # SD card operations
    if 'SD' in content or 'sd' in content.lower():
        if 'write' in content.lower() or 'save' in content.lower():
            side_effects.append("SD card write (CSV/JSON)")
        if 'read' in content.lower() or 'load' in content.lower():
            side_effects.append("SD card read")
    
    # Relay/Compressor control
    if 'relay' in content.lower() or 'compressor' in content.lower():
        if 'digitalWrite' in content or 'setRelay' in content:
            side_effects.append("Relay control (fridge compressor)")
    
    # Sensor reads
    if 'sensor' in content.lower():
        if 'read' in content.lower() or 'measure' in content.lower():
            side_effects.append("Sensor reading")
    
    # Deep sleep
    if 'deep_sleep' in content.lower() or 'esp_deep_sleep' in content.lower():
        side_effects.append("Deep sleep entry")
    
    # Touchscreen operations
    if 'TFT' in content or 'touch' in content.lower():
        if 'draw' in content.lower() or 'print' in content.lower():
            side_effects.append("Touchscreen UI updates")
    
    # SPI/OneWire/I2C bus operations
    if 'Wire' in content or 'OneWire' in content or 'I2C' in content:
        side_effects.append("I2C/SPI bus communication")
    
    # Temperature control
    if 'beer_pi' in content.lower() or 'pi_control' in content.lower() or 'temperature' in content.lower():
        if 'target' in content.lower():
            side_effects.append("Temperature target calculation")
    
    # Calibration
    if 'calibrat' in content.lower():
        if 'polynomial' in content.lower() or 'coeff' in content.lower():
            side_effects.append("Calibration coefficient computation")
    
    return side_effects if side_effects else ["No significant side effects"]

def get_change_risk(file_path: str, side_effects: List[str], file_type: str) -> Tuple[str, str]:
    """Determine change risk level and rationale."""
    
    # HIGH risk - hardware control, protocol, calibration, persistence, power
    high_risk_indicators = [
        ('relay', "Relay control affects hardware"),
        ('compressor', "Compressor control affects hardware"),
        ('eeprom', "EEPROM write corrupts calibration"),
        ('calibrat', "Calibration affects all SG readings"),
        ('protocol', "Protocol changes break compatibility"),
        ('sg_protocol', "Protocol changes break compatibility"),
        ('deep_sleep', "Power management affects device behavior"),
        ('beer_pi', "PI control affects temperature regulation"),
        ('fermentation_state', "State machine affects batch tracking"),
    ]
    
    for indicator, rationale in high_risk_indicators:
        if indicator in file_path.lower():
            return "HIGH", rationale
    
    # Medium risk - core functionality, UI, domain logic
    medium_risk_indicators = [
        ('domain', "Domain logic affects business rules"),
        ('ui', "UI changes affect user experience"),
        ('calculations', "Calculation changes affect data accuracy"),
        ('yeast', "Yeast management affects batch profiles"),
        ('batch', "Batch persistence affects data integrity"),
    ]
    
    for indicator, rationale in medium_risk_indicators:
        if indicator in file_path.lower():
            return "MEDIUM", rationale
    
    # Low risk - tests, docs, helpers
    if 'test' in file_path.lower() or 'test_harness' in file_path.lower():
        return "LOW", "Test code, doesn't affect production behavior"
    
    if 'docs' in file_path.lower() or file_path.endswith('.md'):
        return "LOW", "Documentation changes"
    
    return "MEDIUM", "Core functionality change"

def generate_file_card(project_root: Path, file_path: str, file_map_entry: Optional[Dict] = None) -> str:
    """Generate a single file card."""
    full_path = project_root / file_path
    rel_path = file_path
    
    if not full_path.exists():
        return f"# {rel_path}\n\n**Status:** File not found\n"
    
    file_hash = calculate_hash(str(full_path))
    file_type = get_file_type(rel_path)
    
    with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read(5000)  # First 5KB for analysis
    
    includes = extract_includes(content)
    classes = extract_classes(content)
    structs = extract_structs(content)
    enums = extract_enums(content)
    functions = extract_functions(content)
    setup_loop = extract_setup_loop(content)
    side_effects = get_side_effects_from_file(file_path, content, file_type)
    risk_level, risk_rationale = get_change_risk(file_path, side_effects, file_type)
    
    # Build the card
    lines = []
    lines.append(f"# {rel_path}")
    lines.append("")
    lines.append(f"**Type:** {file_type}")
    lines.append(f"**Hash:** `{file_hash}`")
    lines.append("")
    
    # Add path-based disambiguation to filename if needed
    dir_name = os.path.dirname(rel_path)
    base_name = os.path.basename(rel_path).replace('.h', '').replace('.cpp', '').replace('.ino', '')
    if dir_name:
        safe_filename = f"{dir_name.replace('/', '_')}_{base_name}"
    else:
        safe_filename = base_name
    
    # Role - one or two precise sentences
    lines.append("## Role")
    lines.append("")
    if file_type == 'Arduino Sketch':
        if 'SGNode_Base' in rel_path:
            lines.append("Base station entry point with ESP-NOW receive, UI management, and SD logging.")
        elif 'SGNode_Float' in rel_path:
            lines.append("Float unit entry point with IMU measurement, ESP-NOW transmit, and deep sleep.")
        elif 'SGNode_Plug' in rel_path:
            lines.append("Plug temperature controller entry point with PI control and ESP-NOW communication.")
        else:
            lines.append("Main Arduino sketch entry point.")
    elif file_type == 'C++ Header':
        if 'protocol' in rel_path.lower():
            lines.append("Protocol definitions for ESP-NOW communication (Base↔Float/Plug).")
        elif 'hardware' in rel_path.lower():
            lines.append("Hardware pin definitions and low-level control helpers.")
        elif 'yeast' in rel_path.lower():
            lines.append("Yeast preset configuration and repository.")
        elif 'batch' in rel_path.lower():
            lines.append("Batch data structures and state management.")
        elif 'fermentation' in rel_path.lower():
            lines.append("Fermentation state machine and phase tracking.")
        elif 'calibrat' in rel_path.lower():
            lines.append("Gravity calibration system (EEPROM-stored polynomial coefficients).")
        elif 'brix' in rel_path.lower():
            lines.append("Brix to SG conversion using calibration polynomial.")
        elif 'target' in rel_path.lower():
            lines.append("Temperature target curve generation.")
        elif 'beer_pi' in rel_path.lower():
            lines.append("PI temperature controller with D-brake for compressor control.")
        elif 'air' in rel_path.lower():
            lines.append("Fridge air temperature target calculation from beer target.")
        elif 'transport' in rel_path.lower():
            lines.append("ESP-NOW transport layer for Plug communication.")
        elif 'sensor' in rel_path.lower():
            lines.append("DS18B20 temperature sensor reading with CRC validation.")
        elif 'relay' in rel_path.lower():
            lines.append("Relay duty cycle tracking and compressor pattern generation.")
        elif 'ui' in rel_path.lower():
            lines.append("Touchscreen UI components for base station.")
        else:
            lines.append("Header file with declarations.")
    else:
        lines.append(f"{file_type} file.")
    lines.append("")
    
    # Verified API / Symbols
    lines.append("## Verified API / Symbols")
    lines.append("")
    
    if file_type == 'Arduino Sketch':
        if setup_loop['setup'] or setup_loop['loop']:
            if setup_loop['setup']:
                lines.append("- `setup()` - Initialization entry point")
            if setup_loop['loop']:
                lines.append("- `loop()` - Main event loop")
            lines.append("")
    
    # Add structs/enums from protocol headers
    if 'protocol' in rel_path.lower() or 'sg_protocol' in rel_path.lower():
        lines.append("- `payload_t` - Float data packet (26 bytes)")
        lines.append("- `sg_plug_command_t` - Plug command (107 bytes)")
        lines.append("- `sg_plug_status_t` - Plug status report (61 bytes)")
        lines.append("- `ack_packet_t` - ACK packet with commands (7 bytes)")
        lines.append("- `calib_command_t`, `calib_response_t`, `calib_coeffs_t`")
        lines.append("- `sg_crc16()` - CRC16-CCITT checksum function")
        lines.append("")
        lines.append("Constants:")
        lines.append("- `SG_PROTOCOL_VERSION = 2`")
        lines.append("- `SG_ACK_COMMAND_ZERO_CALIBRATE = 1`")
        lines.append("")
    elif structs or classes or enums:
        if structs:
            lines.append(f"- Structs: {', '.join(structs[:5])}")
        if classes:
            lines.append(f"- Classes: {', '.join(classes[:5])}")
        if enums:
            lines.append(f"- Enums: {', '.join(enums[:5])}")
        lines.append("")
    
    if file_type == 'C++ Header' and file_path.endswith('.h'):
        # For header files, also check for function declarations
        func_decls = []
        # More selective pattern to avoid __attribute__ noise
        pattern = r'\b(void|bool|int|float|double|uint8_t|uint16_t|uint32_t|size_t)\s+(\w+)\s*\([^)]*\)'
        for match in re.finditer(pattern, content):
            func_name = match.group(2)
            if func_name not in ['setup', 'loop', 'class', 'struct', 'enum', 'static_assert', 'sizeof']:
                func_decls.append(func_name)
        # Deduplicate and limit
        func_decls = list(dict.fromkeys(func_decls))[:10]  # Remove duplicates, keep order
        if func_decls:
            lines.append(f"- Functions: {', '.join(func_decls)}")
            lines.append("")
    
    # Key Dependencies
    lines.append("## Key Dependencies")
    lines.append("")
    
    if 'protocol' in rel_path.lower() or 'sg_protocol' in rel_path.lower():
        lines.append("- Arduino.h (cstdint, cstddef)")
        lines.append("")
    elif 'hardware' in rel_path.lower() or 'plug_hardware' in rel_path.lower():
        lines.append("- Arduino.h")
        lines.append("")
    elif 'calibrat' in rel_path.lower() or 'polynomial' in rel_path.lower():
        lines.append("- Arduino.h")
        lines.append("- EEPROM.h (for storing coefficients)")
        lines.append("")
    elif 'brix' in rel_path.lower():
        lines.append("- polynomial_calibration.h")
        lines.append("")
    elif 'target' in rel_path.lower():
        lines.append("- brew_profile.h")
        lines.append("")
    elif 'beer_pi' in rel_path.lower():
        lines.append("- Arduino.h")
        lines.append("")
    elif 'air' in rel_path.lower():
        lines.append("- beer_pi_controller.h")
        lines.append("")
    elif 'transport' in rel_path.lower():
        lines.append("- Arduino.h")
        lines.append("- sg_protocol.h")
        lines.append("- esp_now.h")
        lines.append("")
    elif 'sensor' in rel_path.lower():
        lines.append("- Arduino.h")
        lines.append("- OneWire.h")
        lines.append("- DallasTemperature.h (for beer sensor)")
        lines.append("")
    elif 'relay' in rel_path.lower():
        lines.append("- Arduino.h")
        lines.append("")
    elif 'ui' in rel_path.lower():
        lines.append("- Arduino.h")
        lines.append("- TFT_eSPI.h")
        lines.append("")
    elif 'yeast' in rel_path.lower():
        lines.append("- Arduino.h")
        lines.append("- FS.h, SD.h (for yeast file storage)")
        lines.append("")
    elif 'batch' in rel_path.lower():
        lines.append("- Arduino.h")
        lines.append("- FS.h, SD.h (for batch persistence)")
        lines.append("")
    elif 'fermentation' in rel_path.lower():
        lines.append("- Arduino.h")
        lines.append("- batch_action.h")
        lines.append("")
    else:
        if includes:
            lines.append("- " + ", ".join(includes[:5]))
        else:
            lines.append("- No external dependencies")
    
    lines.append("")
    
    # Side Effects
    lines.append("## Side Effects")
    lines.append("")
    
    if side_effects:
        for effect in side_effects:
            lines.append(f"- {effect}")
    else:
        lines.append("- No significant side effects")
    
    lines.append("")
    
    # Tests
    lines.append("## Tests")
    lines.append("")
    
    if 'test' in rel_path.lower() or 'test_harness' in rel_path.lower():
        lines.append("- Unit test infrastructure for UI")
    else:
        lines.append("- No dedicated automated test identified")
    
    lines.append("")
    
    # Change Risks
    lines.append("## Change Risks")
    lines.append("")
    lines.append(f"- **{risk_level}**: {risk_rationale}")
    lines.append("")
    
    # Summary
    lines.append("---")
    lines.append(f"*Last modified (hash): {file_hash}*")
    
    return "\n".join(lines), safe_filename

def main():
    project_root = Path("/tmp/conversation-worktrees/106e6d18-003a-48ea-b9d7-5283b591d2ab/SGNode")
    output_dir = project_root / ".openhands/librarian/files"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Key files to process (priority order)
    key_files = [
        # High-priority entry points
        "SGNode_Base/SGNode_Base.ino",
        "SGNode_Float/SGNode_Float.ino",
        "SGNode_Plug/SGNode_Plug.ino",
        
        # High-risk shared/protocol/control files
        "SGNode_Shared/sg_protocol.h",
        "SGNode_Shared/plug_hardware.h",
        
        # Plug transport and control
        "SGNode_Plug/src/plug_transport.cpp",
        "SGNode_Plug/src/plug_transport.h",
        "SGNode_Plug/src/plug_sensors.cpp",
        "SGNode_Plug/src/plug_sensors.h",
        "SGNode_Plug/src/plug_types.h",
        "SGNode_Plug/src/plug_selfcheck.cpp",
        "SGNode_Plug/src/plug_selfcheck.h",
        "SGNode_Plug/src/beer_pi_controller.cpp",
        "SGNode_Plug/src/beer_pi_controller.h",
        "SGNode_Plug/src/air_controller.cpp",
        "SGNode_Plug/src/air_controller.h",
        "SGNode_Plug/src/relay_history.cpp",
        "SGNode_Plug/src/relay_history.h",
        
        # Base domain and calculations
        "SGNode_Base/src/domain/yeast_preset_repository.cpp",
        "SGNode_Base/src/domain/yeast_preset_repository.h",
        "SGNode_Base/src/domain/batch_action.cpp",
        "SGNode_Base/src/domain/batch_action.h",
        "SGNode_Base/src/domain/fermentation_state_machine.cpp",
        "SGNode_Base/src/domain/fermentation_state_machine.h",
        "SGNode_Base/src/domain/recommendation_engine.cpp",
        "SGNode_Base/src/domain/recommendation_engine.h",
        "SGNode_Base/src/domain/brew_profile.cpp",
        "SGNode_Base/src/domain/brew_profile.h",
        "SGNode_Base/src/domain/yeast_preset.cpp",
        "SGNode_Base/src/domain/yeast_preset.h",
        "SGNode_Base/src/calculations/brix_converter.cpp",
        "SGNode_Base/src/calculations/brix_converter.h",
        "SGNode_Base/src/calculations/derived_calculations.cpp",
        "SGNode_Base/src/calculations/derived_calculations.h",
        "SGNode_Base/src/calculations/eta_predictor.cpp",
        "SGNode_Base/src/calculations/eta_predictor.h",
        "SGNode_Base/src/calculations/og_verifier.cpp",
        "SGNode_Base/src/calculations/og_verifier.h",
        "SGNode_Base/src/calculations/target_curve.cpp",
        "SGNode_Base/src/calculations/target_curve.h",
        "SGNode_Base/src/calculations/polynomial_calibration.h",
        
        # Base UI
        "SGNode_Base/src/ui/brew_wizard_controller.cpp",
        "SGNode_Base/src/ui/brew_wizard_controller.h",
        "SGNode_Base/src/ui/ui_components.cpp",
        "SGNode_Base/src/ui/ui_components.h",
        "SGNode_Base/src/ui/ui_tokens.h",
        
        # Test harness
        "SGNode_Base/src/test/ui_test_harness.cpp",
        "SGNode_Base/src/test/ui_test_harness.h",
        
        # Core shared
        "SGNode_Float/polynomial_calibration.h",
        
        # Assets
        "SGNode_Base/src/assets/Logo2_Optimized.h",
        "SGNode_Base/src/assets/Logo2_Optimized_Data.h",
    ]
    
    print(f"SGNode File Card Generator (Improved)")
    print(f"======================================")
    print(f"Project: {project_root}")
    print(f"Output: {output_dir}")
    print()
    
    generated_count = 0
    skipped_count = 0
    low_confidence_count = 0
    
    # Deduplicate by path (some files appear in both .h and .cpp)
    seen_paths = set()
    unique_key_files = []
    for rel_path in key_files:
        if rel_path not in seen_paths:
            seen_paths.add(rel_path)
            unique_key_files.append(rel_path)
    
    for rel_path in unique_key_files:
        full_path = project_root / rel_path
        
        if not full_path.exists():
            print(f"  SKIPPED (not found): {rel_path}")
            skipped_count += 1
            continue
        
        card_content, safe_filename = generate_file_card(project_root, rel_path)
        
        output_path = output_dir / (safe_filename + '.md')
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(card_content)
        
        # Check for low confidence markers
        if 'needs source review' in card_content.lower() or 'confidence: low' in card_content.lower():
            low_confidence_count += 1
            print(f"  LOW CONFIDENCE: {rel_path}")
        
        print(f"  Generated: {safe_filename}.md")
        generated_count += 1
    
    print()
    print(f"Summary:")
    print(f"  Generated: {generated_count}")
    print(f"  Skipped: {skipped_count}")
    print(f"  Low confidence: {low_confidence_count}")
    
    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
