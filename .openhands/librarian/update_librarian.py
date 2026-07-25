#!/usr/bin/env python3
"""Incremental librarian memory updater for SGNode.

This script updates librarian memory files only for changed files,
rather than regenerating everything. Use after making code changes.

Usage: python3 .openhands/librarian/update_librarian.py
"""

import os
import sys
import hashlib
import json
from pathlib import Path

def calculate_hash(filepath):
    """Calculate SHA256 hash of a file."""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def get_file_type(filepath):
    """Determine file type/language from extension."""
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

def load_file_map(librarian_dir):
    """Load existing file-map.json."""
    map_path = librarian_dir / 'file-map.json'
    if map_path.exists():
        with open(map_path, 'r') as f:
            return json.load(f)
    return []

def get_changed_files():
    """Get list of changed files from git."""
    import subprocess
    try:
        result = subprocess.run(
            ['git', 'diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD'],
            capture_output=True, text=True, check=True
        )
        return result.stdout.strip().split('\n') if result.stdout.strip() else []
    except subprocess.CalledProcessError:
        return []
    except FileNotFoundError:
        print("Warning: git not found. Running full update.")
        return None

def get_relevant_files(project_root):
    """Get list of all relevant files to track."""
    relevant_extensions = {'.h', '.cpp', '.ino', '.json', '.md', '.py', '.sh'}
    ignore_patterns = {
        '.git', 'node_modules', '.venv', 'venv', 'dist', 'build',
        'target', '.cache', 'coverage', '.3mf'
    }
    
    files = []
    for root, dirs, filenames in os.walk(project_root):
        # Remove ignored directories
        dirs[:] = [d for d in dirs if not any(p in d for p in ignore_patterns)]
        
        for filename in filenames:
            filepath = Path(root) / filename
            rel_path = filepath.relative_to(project_root)
            
            # Check extension
            ext = filepath.suffix.lower()
            if ext not in relevant_extensions:
                continue
            
            # Skip library files
            if 'library' in filepath.parts:
                continue
            
            files.append(str(rel_path))
    
    return files

def find_file_entry(file_map, rel_path):
    """Find existing entry for a file path."""
    for entry in file_map:
        if entry['path'] == rel_path:
            return entry
    return None

def update_file_entry(project_root, file_map, rel_path):
    """Update or create entry for a file."""
    full_path = project_root / rel_path
    
    if not full_path.exists():
        return None
    
    current_hash = calculate_hash(full_path)
    file_type = get_file_type(rel_path)
    
    # Determine role based on path
    if rel_path.startswith('.openhands/'):
        role = 'Agent Memory System'
    elif 'SGNode_Base' in rel_path:
        if rel_path.endswith('.ino'):
            role = 'Base Station Main Entry'
        elif 'ui' in rel_path.lower():
            role = 'User Interface Component'
        elif 'domain' in rel_path.lower():
            role = 'Business Logic / Domain Model'
        elif 'calculations' in rel_path.lower():
            role = 'Calculation / Algorithm'
        elif 'test' in rel_path.lower():
            role = 'Test Harness'
        else:
            role = 'Base Station Component'
    elif 'SGNode_Float' in rel_path:
        if rel_path.endswith('.ino'):
            role = 'Float Unit Main Entry'
        else:
            role = 'Float Unit Component'
    elif 'SGNode_Plug' in rel_path:
        if rel_path.endswith('.ino'):
            role = 'Plug Main Entry'
        else:
            role = 'Plug Component'
    elif 'SGNode_Shared' in rel_path:
        if rel_path.endswith('.h'):
            role = 'Shared Header / Protocol'
        else:
            role = 'Shared Component'
    elif rel_path.startswith('docs/'):
        role = 'Documentation'
    elif rel_path.endswith('.md'):
        role = 'Documentation'
    else:
        role = 'Unknown'
    
    # Determine detail markdown path
    detail_md = rel_path.replace('/', '_').replace('.md', '').replace('.h', '').replace('.cpp', '').replace('.ino', '')
    detail_md = f"files/{detail_md}.md"
    
    return {
        'path': rel_path,
        'role': role,
        'hash': current_hash,
        'detail_md': detail_md,
        'type': file_type
    }

def main():
    script_dir = Path(__file__).parent
    # script_dir is .openhands/librarian, so parent is .openhands, grandparent is project root
    project_root = script_dir.parent.parent
    
    print(f"SGNode Librarian Incremental Update")
    print(f"====================================")
    print(f"Project: {project_root}")
    print(f"Librarian: {script_dir}")
    print()
    
    # Check for git and changed files
    changed_files = get_changed_files()
    
    if changed_files is None:
        print("Running full update (git unavailable)")
        full_update = True
    elif not changed_files or changed_files == ['']:
        print("No changed files detected. Nothing to update.")
        return 0
    else:
        print("Changed files:")
        for f in changed_files:
            print(f"  - {f}")
        print()
        full_update = False
    
    # Load existing file map
    file_map = load_file_map(script_dir)
    print(f"Existing file map entries: {len(file_map)}")
    
    # Build new file map
    # Use a dict to track entries by path (key = path, value = entry)
    new_file_entries = {}
    
    if full_update:
        # Full update: scan all relevant files
        relevant_files = get_relevant_files(project_root)
        print(f"Scanning {len(relevant_files)} relevant files...")
        for rel_path in relevant_files:
            entry = update_file_entry(project_root, file_map, rel_path)
            if entry:
                new_file_entries[rel_path] = entry
    else:
        # Incremental update: only update changed files
        relevant_extensions = {'.h', '.cpp', '.ino', '.json', '.md', '.py', '.sh'}
        
        for rel_path in changed_files:
            # Only process relevant files
            ext = Path(rel_path).suffix.lower()
            if ext not in relevant_extensions:
                continue
            
            if 'library' in rel_path:
                continue
            
            entry = update_file_entry(project_root, file_map, rel_path)
            if entry:
                new_file_entries[rel_path] = entry
                print(f"Updated: {rel_path}")
        
        # Also keep unchanged entries (merge with existing)
        for entry in file_map:
            path = entry['path']
            if path not in new_file_entries:
                new_file_entries[path] = entry
    
    # Convert dict to list, sorted by path
    new_file_map = sorted(new_file_entries.values(), key=lambda x: x['path'])
    
    # Write updated file map
    map_path = script_dir / 'file-map.json'
    with open(map_path, 'w') as f:
        json.dump(new_file_map, f, indent=2)
    
    print()
    print(f"Updated {len(new_file_map)} file entries in file-map.json")
    
    # Generate file cards for all changed files
    files_dir = script_dir / 'files'
    files_dir.mkdir(parents=True, exist_ok=True)
    
    # Import and use the improved generator
    sys.path.insert(0, str(script_dir))
    try:
        from generate_file_cards_improved import generate_file_card
        
        # Determine which files to update
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
        
        # Filter to relevant files (only source files, not markdown/other)
        source_extensions = {'.h', '.cpp', '.ino'}
        files_to_update = [
            f for f in key_files 
            if Path(f).suffix.lower() in source_extensions 
            and (full_update or f in changed_files)
        ]
        
        if files_to_update:
            print(f"\nRegenerating file cards for {len(files_to_update)} changed source files...")
            print(f"SGNode File Card Generator (Improved)")
            print(f"======================================")
            print(f"Project: {project_root}")
            print(f"Output: {files_dir}")
            print()
            
            generated_count = 0
            for rel_path in files_to_update:
                full_path = project_root / rel_path
                
                if not full_path.exists():
                    continue
                
                card_content, safe_filename = generate_file_card(project_root, rel_path)
                
                output_path = files_dir / (safe_filename + '.md')
                
                with open(output_path, 'w', encoding='utf-8') as f:
                    f.write(card_content)
                
                print(f"  Generated: {safe_filename}.md")
                generated_count += 1
            
            print()
            print(f"Generated/updated {generated_count} file cards")
        else:
            print()
            print("No source files to regenerate (only documentation changes)")
    except Exception as e:
        print(f"Warning: Could not generate file cards: {e}")
        import traceback
        traceback.print_exc()
        print("Run python3 generate_file_cards_improved.py manually")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
