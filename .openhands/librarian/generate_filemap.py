#!/usr/bin/env python3
"""Generate file-map.json for SGNode project."""
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

def get_file_type(filepath):
    """Determine file type/language from extension."""
    ext = os.path.splitext(filepath)[1].lower()
    if ext == '.h':
        return 'C++ Header'
    elif ext == '.cpp':
        return 'C++ Source'
    elif ext == '.ino':
        return 'Arduino Sketch'
    elif ext == '.json':
        return 'JSON'
    elif ext == '.md':
        return 'Markdown'
    elif ext == '.py':
        return 'Python'
    elif ext == '.sh':
        return 'Shell'
    else:
        return 'Other'

def is_relevant_file(filepath):
    """Check if file should be included in file map."""
    ignore_patterns = [
        '.git', 'node_modules', '.venv', 'venv', 'dist', 'build', 
        'target', '.cache', 'coverage', '.3mf'
    ]
    
    for pattern in ignore_patterns:
        if pattern in filepath:
            return False
    
    # Only include source files and key documentation
    relevant_extensions = ['.h', '.cpp', '.ino', '.json', '.md', '.py', '.sh']
    ext = os.path.splitext(filepath)[1].lower()
    
    if ext not in relevant_extensions:
        return False
    
    # Skip library files and test harness unless in test directory
    if 'library' in filepath.lower():
        return False
    
    return True

def main():
    project_root = '/tmp/conversation-worktrees/106e6d18-003a-48ea-b9d7-5283b591d2ab/SGNode'
    output_dir = os.path.join(project_root, '.openhands/librarian')
    
    file_map = []
    
    # Walk through project directories
    for root, dirs, files in os.walk(project_root):
        # Skip ignored directories
        dirs[:] = [d for d in dirs if not any(p in d for p in ['node_modules', '.git', '.venv', 'venv'])]
        
        for filename in files:
            filepath = os.path.join(root, filename)
            rel_path = os.path.relpath(filepath, project_root)
            
            if is_relevant_file(rel_path):
                file_hash = calculate_hash(filepath)
                file_type = get_file_type(rel_path)
                
                # Determine role based on path
                if 'AGENTS.md' in rel_path or rel_path == 'README.md':
                    role = 'Project README / Entry Point'
                elif rel_path.startswith('.openhands/'):
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
                
                detail_md = f"files/{rel_path.replace('/', '_').replace('.md', '')}.md" if rel_path.endswith('.md') else f"files/{rel_path.replace('/', '_')}.md"
                
                file_entry = {
                    "path": rel_path,
                    "role": role,
                    "hash": file_hash,
                    "detail_md": detail_md,
                    "type": file_type
                }
                file_map.append(file_entry)
    
    # Sort by path
    file_map.sort(key=lambda x: x['path'])
    
    # Write to file
    output_path = os.path.join(output_dir, 'file-map.json')
    with open(output_path, 'w') as f:
        json.dump(file_map, f, indent=2)
    
    print(f"Generated {len(file_map)} file entries to {output_path}")
    return file_map

if __name__ == '__main__':
    main()
