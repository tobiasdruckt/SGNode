#!/usr/bin/env python3
"""
Improved SGNode file card generator - Version 2
Extracts maximum technical detail from C++/C/Python source files
"""

import os
import hashlib
import re
import json
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Set
from dataclasses import dataclass, field
from collections import defaultdict

@dataclass
class FunctionInfo:
    name: str = ""
    return_type: str = ""
    params: List[str] = field(default_factory=list)
    modifiers: List[str] = field(default_factory=list)
    is_static: bool = False
    is_const: bool = False
    is_override: bool = False
    is_virtual: bool = False
    is_inline: bool = False
    has_noexcept: bool = False
    default_params: List[str] = field(default_factory=list)
    doc_comment: str = ""
    is_private: bool = False
    is_protected: bool = False
    is_public: bool = True

@dataclass
class StructInfo:
    name: str = ""
    fields: List[Tuple[str, str]] = field(default_factory=list)  # (type, name)
    doc_comment: str = ""
    is_packed: bool = False

@dataclass
class ClassInfo:
    name: str = ""
    methods: List[FunctionInfo] = field(default_factory=list)
    members: List[Tuple[str, str, str]] = field(default_factory=list)  # (access, type, name)
    constructors: List[FunctionInfo] = field(default_factory=list)
    destructors: List[FunctionInfo] = field(default_factory=list)
    doc_comment: str = ""
    base_classes: List[str] = field(default_factory=list)
    is_abstract: bool = False

@dataclass
class EnumInfo:
    name: str = ""
    values: List[str] = field(default_factory=list)
    doc_comment: str = ""
    scoped: bool = False

@dataclass
class MacroInfo:
    name: str = ""
    value: str = ""
    params: List[str] = field(default_factory=list)
    doc_comment: str = ""

@dataclass
class TypeInfo:
    name: str = ""
    alias: str = ""
    doc_comment: str = ""

def safe_group(match, group_idx, default=""):
    """Safely get a regex group"""
    try:
        return match.group(group_idx) or default
    except IndexError:
        return default

class CppParser:
    """Comprehensive C++ parser for extracting symbols and documentation"""
    
    def __init__(self, content: str):
        self.content = content
        self.lines = content.split('\n')
        
    def extract_doxygen_comments(self, start_line: int, max_lines: int = 20) -> str:
        """Extract doxygen comments above a line"""
        comments = []
        line_idx = start_line - 1
        
        while line_idx >= 0 and len(comments) < max_lines:
            if line_idx < 0 or line_idx >= len(self.lines):
                break
                
            line = self.lines[line_idx].strip()
            
            # Check for doxygen comment patterns
            if line.startswith('///') or line.startswith('//!'):
                comments.insert(0, line[3:].strip())
                line_idx -= 1
            elif line.startswith('/**') or line.startswith('/*!'):
                # Multi-line doxygen
                while line_idx >= 0:
                    if line_idx < 0 or line_idx >= len(self.lines):
                        break
                    line = self.lines[line_idx].strip()
                    if line.startswith('/**') or line.startswith('/*!'):
                        content = line[3:]
                        if content.endswith('*/'):
                            content = content[:-2]
                        if content.strip():
                            comments.insert(0, content.strip())
                        line_idx -= 1
                        break
                    elif '*/' in line:
                        content = line.split('*/')[0]
                        if content.strip():
                            comments.insert(0, content.strip())
                        line_idx -= 1
                        break
                    else:
                        content = line
                        if content.startswith(' *'):
                            content = content[2:]
                        if content.strip():
                            comments.insert(0, content.strip())
                        line_idx -= 1
                        break
                break
            elif line.startswith('*') and not line.startswith('*/'):
                # Comment continuation
                comments.insert(0, line[1:].strip())
                line_idx -= 1
            elif line.startswith('//'):
                # Regular comment - stop here
                break
            elif not line:
                line_idx -= 1
            else:
                # Non-comment line
                break
        
        return '\n'.join(comments).strip()
    
    def find_brace_pair(self, start_pos: int) -> int:
        """Find matching closing brace"""
        brace_count = 1
        pos = start_pos
        while pos < len(self.content) and brace_count > 0:
            if self.content[pos] == '{':
                brace_count += 1
            elif self.content[pos] == '}':
                brace_count -= 1
            pos += 1
        return pos
    
    def extract_structs(self) -> List[StructInfo]:
        """Extract struct definitions with fields"""
        structs = []
        seen_structs = set()  # Track seen struct names to avoid duplicates
        
        # Pattern 1: struct Name { ... }
        struct_pattern1 = r'struct\s+__attribute__\s*\(\s*\(packed\)\s*\)\s*(\w+)\s*\{'
        
        for match in re.finditer(struct_pattern1, self.content):
            name = match.group(1)
            if name in seen_structs:
                continue
            seen_structs.add(name)
            
            start_pos = match.end() - 1  # Position of opening brace
            end_pos = self.find_brace_pair(start_pos + 1)
            body = self.content[start_pos + 1:end_pos - 1]
            
            # Extract fields
            fields = []
            for line in body.split('\n'):
                line = line.strip()
                if not line or line.startswith('//') or line.startswith('#'):
                    continue
                
                # Skip methods
                if '(' in line and ')' in line:
                    continue
                
                # Remove trailing comments
                if '//' in line:
                    line = line.split('//')[0].strip()
                
                # Skip if doesn't end with semicolon
                if not line.endswith(';'):
                    continue
                
                # Remove semicolon
                line_content = line[:-1].strip()
                if not line_content:
                    continue
                
                # Parse field: type name;
                parts = line_content.split()
                if len(parts) >= 2:
                    field_type = ' '.join(parts[:-1])
                    field_name = parts[-1].strip()
                    if field_name and not field_name.startswith('static'):
                        fields.append((field_type, field_name))
            
            doc_comment = self.extract_doxygen_comments(match.start())
            
            struct_info = StructInfo(
                name=name,
                fields=fields,
                doc_comment=doc_comment,
                is_packed=True
            )
            structs.append(struct_info)
        
        # Pattern 2: typedef struct { ... } Name;
        typedef_pattern = r'typedef\s+struct\s+(?:__attribute__\s*\(\s*\(packed\)\s*\)\s*)?\{([^}]+)\}\s*(\w+);'
        for match in re.finditer(typedef_pattern, self.content, re.DOTALL):
            body = match.group(1)
            name = match.group(2)
            if name in seen_structs:
                continue
            seen_structs.add(name)
            
            fields = []
            for line in body.split('\n'):
                line = line.strip()
                if not line or line.startswith('//') or line.startswith('#'):
                    continue
                if '(' in line and ')' in line:
                    continue
                if '//' in line:
                    line = line.split('//')[0].strip()
                if not line.endswith(';'):
                    continue
                line_content = line[:-1].strip()
                if not line_content:
                    continue
                parts = line_content.split()
                if len(parts) >= 2:
                    field_type = ' '.join(parts[:-1])
                    field_name = parts[-1].strip()
                    if field_name and not field_name.startswith('static'):
                        fields.append((field_type, field_name))
            
            doc_comment = self.extract_doxygen_comments(match.start())
            
            struct_info = StructInfo(
                name=name,
                fields=fields,
                doc_comment=doc_comment,
                is_packed='packed' in match.group(0)
            )
            structs.append(struct_info)
        
        # Pattern 3: struct { ... } Name; (anonymous struct with name)
        anonymous_struct_pattern = r'struct\s+\{([^}]+)\}\s*(\w+)\s*;'
        for match in re.finditer(anonymous_struct_pattern, self.content, re.DOTALL):
            body = match.group(1)
            name = match.group(2)
            if name in seen_structs:
                continue
            seen_structs.add(name)
            
            fields = []
            for line in body.split('\n'):
                line = line.strip()
                if not line or line.startswith('//') or line.startswith('#'):
                    continue
                if '(' in line and ')' in line:
                    continue
                if '//' in line:
                    line = line.split('//')[0].strip()
                if not line.endswith(';'):
                    continue
                line_content = line[:-1].strip()
                if not line_content:
                    continue
                parts = line_content.split()
                if len(parts) >= 2:
                    field_type = ' '.join(parts[:-1])
                    field_name = parts[-1].strip()
                    if field_name and not field_name.startswith('static'):
                        fields.append((field_type, field_name))
            
            doc_comment = self.extract_doxygen_comments(match.start())
            
            struct_info = StructInfo(
                name=name,
                fields=fields,
                doc_comment=doc_comment,
                is_packed=False
            )
            structs.append(struct_info)
        
        # Pattern 4: struct Name { ... } without __attribute__
        struct_pattern2 = r'(?<!typedef\s)struct\s+(\w+)\s*\{'
        
        for match in re.finditer(struct_pattern2, self.content):
            name = match.group(1)
            if name in seen_structs:
                continue
            seen_structs.add(name)
            
            start_pos = match.end() - 1
            end_pos = self.find_brace_pair(start_pos + 1)
            body = self.content[start_pos + 1:end_pos - 1]
            
            fields = []
            for line in body.split('\n'):
                line = line.strip()
                if not line or line.startswith('//') or line.startswith('#'):
                    continue
                if '(' in line and ')' in line:
                    continue
                if '//' in line:
                    line = line.split('//')[0].strip()
                if not line.endswith(';'):
                    continue
                line_content = line[:-1].strip()
                if not line_content:
                    continue
                parts = line_content.split()
                if len(parts) >= 2:
                    field_type = ' '.join(parts[:-1])
                    field_name = parts[-1].strip()
                    if field_name and not field_name.startswith('static'):
                        fields.append((field_type, field_name))
            
            doc_comment = self.extract_doxygen_comments(match.start())
            
            struct_info = StructInfo(
                name=name,
                fields=fields,
                doc_comment=doc_comment,
                is_packed=False
            )
            structs.append(struct_info)
        
        return structs
    
    def extract_classes(self) -> List[ClassInfo]:
        """Extract class definitions with methods and members"""
        classes = []
        
        # Find all class declarations
        class_pattern = r'class\s+(\w+)'
        
        for match in re.finditer(class_pattern, self.content):
            name = match.group(1)
            start_pos = match.end()
            
            # Find opening brace
            brace_pos = self.content.find('{', start_pos)
            if brace_pos == -1:
                continue
            
            # Find matching closing brace
            end_pos = self.find_brace_pair(brace_pos + 1)
            body = self.content[brace_pos + 1:end_pos - 1]
            
            methods = []
            members = []
            constructors = []
            destructors = []
            
            # Extract doc comment before the class
            doc_comment = self.extract_doxygen_comments(match.start())
            
            # Parse the body line by line
            self._parse_class_body_simple(body, methods, members, constructors, destructors)
            
            class_info = ClassInfo(
                name=name,
                methods=methods,
                members=members,
                constructors=constructors,
                destructors=destructors,
                doc_comment=doc_comment,
                is_abstract='= 0' in body
            )
            classes.append(class_info)
        
        return classes
    
    def _parse_class_body_simple(self, body: str, methods: List, members: List, 
                                constructors: List, destructors: List):
        """Simple parser for class body"""
        lines = body.split('\n')
        current_access = "private"  # Default access for C++ classes
        
        for line in lines:
            line = line.strip()
            
            # Check for access specifiers
            if line in ['public:', 'protected:', 'private:']:
                current_access = line[:-1]
                continue
            
            # Skip empty lines and comments
            if not line or line.startswith('//') or line.startswith('#'):
                continue
            
            # Skip if it's a method definition with body
            if '{' in line and '}' in line and line.endswith(';'):
                continue
            
            # Check for method declarations (end with ; not {)
            if line.endswith(';'):
                # Try to extract function signature
                # Pattern: [modifiers] return_type name(params) [modifiers];
                func_match = re.match(
                    r'^(?:static\s+)?(?:inline\s+)?(?:virtual\s+)?(?:explicit\s+)?'
                    r'(\w+(?:\s+const)?(?:\s+long)?(?:\s+short)?(?:\s+signed)?(?:\s+unsigned)?(?:\s+int)?(?:\s+char)?(?:\s+float)?(?:\s+double)?(?:\s+bool)?)\s*'
                    r'(\*?\s*\w+)\s*'
                    r'\(([^)]*)\)\s*'
                    r'(?:const\s*)?(?:override\s*)?(?:final\s*)?(?:noexcept\s*)?',
                    line
                )
                
                if func_match:
                    return_type = func_match.group(1).strip()
                    name = func_match.group(2).strip()
                    params_str = func_match.group(3).strip()
                    
                    # Skip keywords
                    if name in ['if', 'while', 'for', 'switch', 'catch', 'return', 'sizeof', 'decltype', 'typeid']:
                        continue
                    
                    # Parse parameters
                    params = []
                    if params_str:
                        param_list = params_str.split(',')
                        for param in param_list:
                            param = param.strip()
                            if param:
                                params.append(param)
                    
                    # Check if constructor or destructor
                    if name.startswith('~'):
                        # Destructor
                        func_info = FunctionInfo(
                            name=name,
                            return_type=return_type,
                            params=params,
                            is_public=(current_access == 'public'),
                            is_protected=(current_access == 'protected'),
                            is_private=(current_access == 'private')
                        )
                        destructors.append(func_info)
                    elif not return_type:
                        # Constructor (no return type)
                        func_info = FunctionInfo(
                            name=name,
                            params=params,
                            is_public=(current_access == 'public'),
                            is_protected=(current_access == 'protected'),
                            is_private=(current_access == 'private')
                        )
                        constructors.append(func_info)
                    else:
                        # Regular method
                        is_static = 'static' in line
                        is_const = 'const' in line and 'const' in return_type
                        is_override = 'override' in line
                        
                        func_info = FunctionInfo(
                            name=name,
                            return_type=return_type,
                            params=params,
                            modifiers=['static'] if is_static else [],
                            is_static=is_static,
                            is_const=is_const,
                            is_override=is_override,
                            is_public=(current_access == 'public'),
                            is_protected=(current_access == 'protected'),
                            is_private=(current_access == 'private')
                        )
                        methods.append(func_info)
                else:
                    # Not a function, might be a variable
                    var_match = re.match(r'^(?:static\s+)?(?:const\s+)?(\w+(?:\s+const)?)(?:\s+\*?)\s+(\w+)\s*;', line)
                    if var_match:
                        var_type = var_match.group(1).strip()
                        var_name = var_match.group(2).strip()
                        members.append((current_access, var_type, var_name))
    
    def extract_functions(self) -> List[FunctionInfo]:
        """Extract standalone functions"""
        functions = []
        
        # Pattern for function definitions (with bodies) - more flexible
        func_pattern = r'(\w+)\s*\(([^)]*)\)\s*\{'
        
        for match in re.finditer(func_pattern, self.content):
            name = match.group(1).strip()
            params_str = match.group(2).strip()
            
            # Skip control structures and keywords
            if name in ['if', 'while', 'for', 'switch', 'catch', 'return', 'sizeof', 'decltype', 'typeid']:
                continue
            
            # Skip if it's inside a class method (will be caught by class parsing)
            start_pos = match.start()
            before_text = self.content[:start_pos]
            # Look for class definition that contains this function
            class_pos = before_text.rfind('{')
            if class_pos != -1:
                # Check if there's a class keyword before the opening brace
                class_match = re.search(r'class\s+\w+\s*$', before_text[:class_pos+1])
                if class_match:
                    continue
            
            # Get context before the function to extract return type
            # Look back up to the opening brace of the function
            context_end = start_pos
            context_start = max(0, context_end - 100)
            
            # Find the opening brace of the function
            brace_pos = self.content.rfind('{', context_start, context_end)
            if brace_pos != -1:
                context = self.content[context_start:brace_pos].strip()
            else:
                context = self.content[context_start:start_pos].strip()
            
            # Extract return type from context
            return_type = "void"
            context_parts = context.split()
            if context_parts:
                # Get the return type (everything before the function name)
                # Find the function name in context
                for i in range(len(context_parts) - 1, -1, -1):
                    if context_parts[i] == name and i > 0:
                        return_type = ' '.join(context_parts[:i]).strip()
                        break
            
            # Parse parameters
            params = []
            default_params = []
            if params_str:
                param_list = params_str.split(',')
                for param in param_list:
                    param = param.strip()
                    if not param:
                        continue
                    
                    # Check for default value
                    default_val = None
                    if '=' in param:
                        parts = param.split('=', 1)
                        param = parts[0].strip()
                        default_val = parts[1].strip()
                    
                    params.append(param)
                    if default_val:
                        default_params.append(default_val)
            
            # Check modifiers
            is_static = 'static' in match.group(0)
            is_const = 'const' in match.group(0)
            is_override = 'override' in match.group(0)
            is_virtual = 'virtual' in match.group(0)
            is_inline = 'inline' in match.group(0)
            has_noexcept = 'noexcept' in match.group(0)
            
            doc_comment = self.extract_doxygen_comments(match.start())
            
            func_info = FunctionInfo(
                name=name,
                return_type=return_type,
                params=params,
                default_params=default_params,
                modifiers=['static'] if is_static else [],
                is_static=is_static,
                is_const=is_const,
                is_override=is_override,
                is_virtual=is_virtual,
                is_inline=is_inline,
                has_noexcept=has_noexcept,
                doc_comment=doc_comment,
                is_public=True
            )
            functions.append(func_info)
        
        return functions
    
    def extract_enums(self) -> List[EnumInfo]:
        """Extract enum definitions"""
        enums = []
        
        # Pattern for enum definitions
        enum_pattern = r'enum\s+(?:class\s+)?(\w+)?\s*\{([^}]+)\}'
        
        for match in re.finditer(enum_pattern, self.content, re.DOTALL):
            name = match.group(1) if match.group(1) else ""
            body = match.group(2)
            
            # Extract values
            values = []
            for line in body.split(','):
                line = line.strip()
                if not line or line.startswith('//'):
                    continue
                
                # Remove trailing comments and whitespace
                if '//' in line:
                    line = line.split('//')[0].strip()
                
                if '=' in line:
                    # Has value
                    parts = line.split('=')
                    value_name = parts[0].strip()
                else:
                    value_name = line
                
                if value_name and value_name not in ['if', 'while', 'for', 'switch']:
                    values.append(value_name)
            
            doc_comment = self.extract_doxygen_comments(match.start())
            
            enum_info = EnumInfo(
                name=name if name else "",
                values=values,
                doc_comment=doc_comment,
                scoped='class' in match.group(0)
            )
            enums.append(enum_info)
        
        return enums
    
    def extract_macros(self) -> List[MacroInfo]:
        """Extract #define macros"""
        macros = []
        
        # Pattern for macros
        macro_pattern = r'#\s*define\s+(\w+)(?:\s*\(([^)]*)\))?\s+(.+?)(?=\n|#)'
        
        for match in re.finditer(macro_pattern, self.content):
            name = match.group(1)
            params_str = match.group(2)
            value = match.group(3).strip()
            
            params = []
            if params_str:
                params = [p.strip() for p in params_str.split(',') if p.strip()]
            
            doc_comment = self.extract_doxygen_comments(match.start())
            
            macro_info = MacroInfo(
                name=name,
                value=value,
                params=params,
                doc_comment=doc_comment
            )
            macros.append(macro_info)
        
        return macros
    
    def extract_typedefs(self) -> List[TypeInfo]:
        """Extract typedef and using statements"""
        types = []
        
        # Pattern for typedef
        typedef_pattern = r'typedef\s+([^;]+);'
        
        for match in re.finditer(typedef_pattern, self.content):
            typedef_content = match.group(1).strip()
            
            # Extract alias and type
            if '*' in typedef_content:
                # Pointer type
                parts = typedef_content.rsplit('*', 1)
                alias = parts[1].strip()
                type_def = parts[0].strip()
            else:
                parts = typedef_content.split()
                if len(parts) >= 2:
                    alias = parts[-1]
                    type_def = ' '.join(parts[:-1])
                else:
                    continue
            
            doc_comment = self.extract_doxygen_comments(match.start())
            
            type_info = TypeInfo(
                name=type_def,
                alias=alias,
                doc_comment=doc_comment
            )
            types.append(type_info)
        
        # Pattern for using (C++)
        using_pattern = r'using\s+(\w+)\s*=\s*([^;]+);'
        
        for match in re.finditer(using_pattern, self.content):
            alias = match.group(1)
            type_def = match.group(2).strip()
            
            doc_comment = self.extract_doxygen_comments(match.start())
            
            type_info = TypeInfo(
                name=type_def,
                alias=alias,
                doc_comment=doc_comment
            )
            types.append(type_info)
        
        return types
    
    def extract_includes(self) -> Tuple[List[str], List[str]]:
        """Extract system and local includes separately"""
        system_includes = []
        local_includes = []
        
        include_pattern = r'#\s*include\s*[<"]([^>"]+)[>"]'
        
        for match in re.finditer(include_pattern, self.content):
            include_path = match.group(1)
            
            if '<' in match.group(0):
                system_includes.append(include_path)
            else:
                local_includes.append(include_path)
        
        return system_includes, local_includes


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
        '.hpp': 'C++ Header',
        '.cpp': 'C++ Source',
        '.ino': 'Arduino Sketch',
        '.c': 'C Source',
        '.json': 'JSON',
        '.md': 'Markdown',
        '.py': 'Python',
        '.sh': 'Shell'
    }
    return type_map.get(ext, 'Other')


def resolve_local_include(include_path: str, file_dir: str, project_root: Path) -> Optional[str]:
    """Resolve local include path to actual file relative to project root"""
    include_file = Path(include_path)
    
    # Try relative to current file's directory
    current_dir = Path(file_dir)
    resolved = current_dir / include_file
    full_path = project_root / resolved
    
    if full_path.exists():
        return str(resolved).replace('\\', '/')
    
    # Try in SGNode_Shared
    resolved = Path("SGNode_Shared") / include_file
    full_path = project_root / resolved
    if full_path.exists():
        return str(resolved).replace('\\', '/')
    
    # Try in same directory with src/
    resolved = Path(file_dir) / include_file
    if 'src' not in str(resolved):
        resolved = Path(file_dir.replace('src/', '')) / include_file
    full_path = project_root / resolved
    if full_path.exists():
        return str(resolved).replace('\\', '/')
    
    # Try without path prefix
    resolved = Path(include_path)
    full_path = project_root / resolved
    if full_path.exists():
        return str(resolved).replace('\\', '/')
    
    return None


def extract_role_from_comment(doc_comment: str) -> str:
    """Extract role/description from doxygen comment"""
    if not doc_comment:
        return "No documentation comment found"
    
    lines = doc_comment.split('\n')
    
    # Extract first sentence or description
    for line in lines:
        line = line.strip()
        if not line:
            continue
        
        # Skip @commands and special tags
        if line.startswith('@') or line.startswith('\\'):
            continue
        
        # Return first non-empty line
        return line
    
    return doc_comment[:200] if len(doc_comment) > 200 else doc_comment


def get_side_effects(content: str, file_type: str, file_path: str) -> List[str]:
    """Extract actual side effects from the file content"""
    side_effects = []
    
    # ESP-NOW operations
    if 'esp_now' in content.lower() or 'esp-now' in content.lower():
        if 'send' in content.lower() or 'esp_now_send' in content:
            side_effects.append("ESP-NOW packet transmission")
        if 'receive' in content.lower() or 'esp_now_register_recv_cb' in content:
            side_effects.append("ESP-NOW packet reception")
        if 'add_peer' in content.lower():
            side_effects.append("ESP-NOW peer registration")
    
    # EEPROM operations
    if 'eeprom' in content.lower():
        if 'write' in content.lower() or 'put' in content.lower():
            side_effects.append("EEPROM write (persistent storage)")
        if 'get' in content.lower() or 'read' in content.lower():
            side_effects.append("EEPROM read")
        if 'begin' in content.lower() and 'eeprom' in content.lower():
            side_effects.append("EEPROM initialization")
        if 'commit' in content.lower():
            side_effects.append("EEPROM commit (flush)")
    
    # SD card operations
    if 'sd' in content.lower() or 'sdcard' in content.lower() or 'sd_card' in content.lower():
        if 'write' in content.lower() or 'save' in content.lower():
            side_effects.append("SD card write (CSV/JSON)")
        if 'read' in content.lower() or 'load' in content.lower():
            side_effects.append("SD card read")
        if 'open' in content.lower() and 'file' in content.lower():
            side_effects.append("SD card file operations")
    
    # Relay/Compressor control
    if 'relay' in content.lower() or 'compressor' in content.lower():
        if 'digitalwrite' in content.lower():
            side_effects.append("Relay control (fridge compressor)")
    
    # Sensor reads
    if 'sensor' in content.lower():
        if 'read' in content.lower() or 'sample' in content.lower() or 'measure' in content.lower():
            side_effects.append("Sensor reading")
    
    # Deep sleep
    if 'deep_sleep' in content.lower() or 'esp_deep_sleep' in content.lower() or 'esp_sleep' in content.lower():
        side_effects.append("Deep sleep entry")
    
    # Touchscreen operations
    if 'tft' in content.lower() or 'touch' in content.lower():
        if 'draw' in content.lower() or 'print' in content.lower() or 'setcursor' in content.lower():
            side_effects.append("Touchscreen UI updates")
    
    # SPI/I2C/OneWire bus operations
    if 'wire' in content.lower() and 'i2c' not in content.lower():
        side_effects.append("I2C bus communication")
    if 'onewire' in content.lower():
        side_effects.append("OneWire bus communication")
    if 'spi' in content.lower():
        side_effects.append("SPI bus communication")
    
    # Temperature control
    if 'beer_pi' in content.lower() or 'pi_control' in content.lower() or 'temperature' in content.lower():
        if 'target' in content.lower():
            side_effects.append("Temperature target calculation")
    
    # Calibration
    if 'calibrat' in content.lower():
        if 'polynomial' in content.lower() or 'coeff' in content.lower():
            side_effects.append("Calibration coefficient computation")
        if 'save' in content.lower() or 'store' in content.lower():
            side_effects.append("Calibration data persistence")
    
    # WiFi operations
    if 'wifi' in content.lower():
        if 'mode' in content.lower() or 'connect' in content.lower():
            side_effects.append("WiFi mode configuration")
        if 'softap' in content.lower():
            side_effects.append("WiFi access point mode")
    
    # Delay/sleep operations
    if 'delay' in content.lower():
        side_effects.append("Execution delay")
    
    # Serial communication
    if 'serial' in content.lower():
        if 'begin' in content.lower():
            side_effects.append("Serial port initialization")
        if 'print' in content.lower() or 'println' in content.lower():
            side_effects.append("Serial output")
    
    return side_effects if side_effects else ["No significant side effects"]


def get_change_risk(file_path: str, side_effects: List[str], content: str) -> Tuple[str, str]:
    """Determine change risk level and rationale"""
    
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
        ('plug_transport', "ESP-NOW transport affects communication"),
        ('plug_sensors', "Sensor reading affects data accuracy"),
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
        ('brew_profile', "Brew profile affects batch configuration"),
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


def format_function_signature(func: FunctionInfo) -> str:
    """Format a function signature with all details"""
    parts = []
    
    # Return type
    if func.return_type:
        parts.append(func.return_type)
    
    # Name
    parts.append(func.name)
    
    # Parameters
    params_str = ', '.join(func.params) if func.params else 'void'
    if func.default_params:
        # Add default values where applicable
        full_params = []
        for i, param in enumerate(func.params):
            if i >= len(func.params) - len(func.default_params):
                default_idx = i - (len(func.params) - len(func.default_params))
                if default_idx < len(func.default_params):
                    # Remove default from param if present
                    param = param.split('=')[0].strip()
                    param = f"{param} = {func.default_params[default_idx]}"
            full_params.append(param)
        params_str = ', '.join(full_params)
    
    parts.append(f"({params_str})")
    
    # Modifiers
    modifiers = []
    if func.is_const:
        modifiers.append("const")
    if func.is_override:
        modifiers.append("override")
    if func.is_static:
        modifiers.append("static")
    if func.is_virtual:
        modifiers.append("virtual")
    if func.has_noexcept:
        modifiers.append("noexcept")
    if func.is_inline:
        modifiers.append("inline")
    
    if modifiers:
        parts.append(' '.join(modifiers))
    
    return ' '.join(parts)


def format_struct_info(struct: StructInfo) -> List[str]:
    """Format struct info with fields"""
    lines = []
    lines.append(f"- `struct {struct.name}`")
    
    if struct.is_packed:
        lines.append("  - (packed)")
    
    if struct.doc_comment:
        lines.append(f"  - {struct.doc_comment.split(chr(10))[0]}")
    
    if struct.fields:
        for field_type, field_name in struct.fields:
            lines.append(f"  - `{field_type} {field_name}`")
    else:
        lines.append("  - No public fields")
    
    return lines


def format_class_info(class_info: ClassInfo) -> List[str]:
    """Format class info with methods and members"""
    lines = []
    lines.append(f"- `class {class_info.name}`")
    
    if class_info.doc_comment:
        lines.append(f"  - {class_info.doc_comment.split(chr(10))[0]}")
    
    if class_info.base_classes:
        lines.append(f"  - Inherits: {', '.join(class_info.base_classes)}")
    
    # Constructors
    if class_info.constructors:
        for ctor in class_info.constructors:
            sig = format_function_signature(ctor)
            lines.append(f"  - {sig}")
    
    # Destructors
    if class_info.destructors:
        for dtor in class_info.destructors:
            sig = format_function_signature(dtor)
            lines.append(f"  - {sig}")
    
    # Public methods
    public_methods = [f for f in class_info.methods if f.is_public]
    if public_methods:
        for func in public_methods:
            sig = format_function_signature(func)
            lines.append(f"  - {sig}")
    
    # Protected methods
    protected_methods = [f for f in class_info.methods if f.is_protected]
    if protected_methods:
        lines.append("  - [protected]")
        for func in protected_methods:
            sig = format_function_signature(func)
            lines.append(f"  - {sig}")
    
    # Private methods
    private_methods = [f for f in class_info.methods if f.is_private]
    if private_methods:
        lines.append("  - [private]")
        for func in private_methods:
            sig = format_function_signature(func)
            lines.append(f"  - {sig}")
    
    # Members
    if class_info.members:
        lines.append("  - Member variables:")
        for access, member_type, member_name in class_info.members:
            lines.append(f"    - `{access}: {member_type} {member_name}`")
    
    return lines


def format_enum_info(enum_info: EnumInfo) -> List[str]:
    """Format enum info with values"""
    lines = []
    name = f"enum {enum_info.name}" if enum_info.name else "enum"
    if enum_info.scoped:
        name = "enum class " + name
    
    lines.append(f"- {name}")
    
    if enum_info.doc_comment:
        lines.append(f"  - {enum_info.doc_comment.split(chr(10))[0]}")
    
    if enum_info.values:
        for value in enum_info.values:
            lines.append(f"  - `{value}`")
    else:
        lines.append("  - No enum values found")
    
    return lines


def format_macro_info(macro: MacroInfo) -> str:
    """Format macro definition"""
    if macro.params:
        params = ', '.join(macro.params)
        return f"- `#define {macro.name}({params}) {macro.value}`"
    else:
        return f"- `#define {macro.name} {macro.value}`"


def generate_file_card(project_root: Path, file_path: str, 
                       file_map_entry: Optional[Dict] = None) -> Tuple[str, str]:
    """Generate a single file card with maximum detail"""
    full_path = project_root / file_path
    rel_path = file_path
    safe_filename = rel_path.replace('/', '_').replace('\\', '_').replace('.', '_')
    
    # Calculate hash
    file_hash = calculate_hash(str(full_path))
    
    # Read entire file (no 5KB limit)
    with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    file_type = get_file_type(file_path)
    
    # Parse with comprehensive parser
    parser = CppParser(content)
    
    # Extract all symbols
    structs = parser.extract_structs()
    classes = parser.extract_classes()
    functions = parser.extract_functions()
    enums = parser.extract_enums()
    macros = parser.extract_macros()
    typedefs = parser.extract_typedefs()
    system_includes, local_includes = parser.extract_includes()
    
    # Resolve local includes
    resolved_local = []
    for inc in local_includes:
        resolved = resolve_local_include(inc, str(Path(file_path).parent), project_root)
        if resolved:
            resolved_local.append(resolved)
        else:
            resolved_local.append(inc)
    
    # Determine role from documentation
    # Prioritize: class doc, then first function doc, then file-level doc
    role = "No documentation found"
    if classes and classes[0].doc_comment:
        role = extract_role_from_comment(classes[0].doc_comment)
    elif structs and structs[0].doc_comment:
        role = extract_role_from_comment(structs[0].doc_comment)
    elif functions and functions[0].doc_comment:
        role = extract_role_from_comment(functions[0].doc_comment)
    
    # If still no role, use file path context
    if role == "No documentation found":
        path_lower = rel_path.lower()
        if 'protocol' in path_lower:
            role = "Protocol definitions for ESP-NOW communication (Base↔Float/Plug)"
        elif 'hardware' in path_lower:
            role = "Hardware pin definitions and inline functions"
        elif 'transport' in path_lower:
            role = "ESP-NOW transport layer for Plug communication"
        elif 'sensor' in path_lower:
            role = "Sensor sampling and validation"
        elif 'controller' in path_lower:
            role = "Control logic for temperature or other systems"
        elif 'state' in path_lower:
            role = "State machine for fermentation tracking"
        elif 'yeast' in path_lower:
            role = "Yeast preset management and persistence"
        elif 'batch' in path_lower:
            role = "Batch profile management and persistence"
        elif 'brew_profile' in path_lower:
            role = "Brew profile data structure and persistence"
        elif 'fermentation' in path_lower:
            role = "Fermentation state tracking and phase detection"
        elif 'calculations' in path_lower:
            role = "Mathematical calculations for brewing metrics"
        elif 'polynomial' in path_lower:
            role = "3rd degree polynomial calibration for gravity measurement"
        elif 'ui' in path_lower:
            role = "User interface components and rendering"
        elif 'test' in path_lower:
            role = "Test infrastructure and validation"
        else:
            role = "Core functionality module"
    
    # Side effects
    side_effects = get_side_effects(content, file_type, file_path)
    
    # Change risk
    risk_level, risk_rationale = get_change_risk(file_path, side_effects, content)
    
    # Build output
    lines = []
    
    # Header
    lines.append(f"# {rel_path}")
    lines.append("")
    lines.append(f"**Type:** {file_type}")
    lines.append(f"**Hash:** `{file_hash}`")
    lines.append("")
    
    # Overview/Role
    lines.append("## Overview / Role")
    lines.append("")
    lines.append(role)
    lines.append("")
    
    # Verified API & Symbols
    lines.append("## Verified API & Symbols")
    lines.append("")
    
    has_any_symbols = False
    
    # Structs & Classes
    if structs or classes:
        lines.append("### Structs & Classes")
        lines.append("")
        has_any_symbols = True
        
        for struct in structs:
            lines.extend(format_struct_info(struct))
            lines.append("")
        
        for class_info in classes:
            lines.extend(format_class_info(class_info))
            lines.append("")
    
    # Functions
    if functions:
        lines.append("### Functions")
        lines.append("")
        has_any_symbols = True
        
        for func in functions:
            if func.doc_comment:
                # Show first line of doc comment
                lines.append(f"<!-- {func.doc_comment.split(chr(10))[0]} -->")
            lines.append(f"- `{format_function_signature(func)}`")
            lines.append("")
    
    # Enums
    if enums:
        lines.append("### Enums & Constants")
        lines.append("")
        has_any_symbols = True
        
        for enum_info in enums:
            lines.extend(format_enum_info(enum_info))
            lines.append("")
    
    # Macros
    if macros:
        lines.append("### Macros")
        lines.append("")
        has_any_symbols = True
        
        for macro in macros:
            lines.append(format_macro_info(macro))
            lines.append("")
    
    # Typedefs/Type aliases
    if typedefs:
        lines.append("### Type Aliases")
        lines.append("")
        has_any_symbols = True
        
        for type_info in typedefs:
            lines.append(f"- `using {type_info.alias} = {type_info.name}`")
            if type_info.doc_comment:
                lines.append(f"  - {type_info.doc_comment.split(chr(10))[0]}")
            lines.append("")
    
    if not has_any_symbols:
        lines.append("No API symbols found or file type not supported")
        lines.append("")
    
    # Key Dependencies
    lines.append("## Key Dependencies")
    lines.append("")
    
    if resolved_local or system_includes:
        # Local dependencies
        if resolved_local:
            lines.append("**Local:**")
            for inc in resolved_local[:10]:  # Limit to 10
                lines.append(f"- `{inc}`")
            lines.append("")
        
        # System/External dependencies
        if system_includes:
            lines.append("**System/External:**")
            for inc in system_includes[:10]:  # Limit to 10
                lines.append(f"- `<{inc}>`")
            lines.append("")
    else:
        lines.append("- No external dependencies")
        lines.append("")
    
    # Side Effects & Hardware Access
    lines.append("## Side Effects & Hardware Access")
    lines.append("")
    
    if side_effects:
        for effect in side_effects:
            lines.append(f"- {effect}")
    else:
        lines.append("- No significant side effects")
    
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


def discover_files(project_root: Path) -> List[str]:
    """Discover all relevant files in the project"""
    files = []
    
    # Define patterns for source files
    patterns = [
        "*.h", "*.hpp", "*.cpp", "*.ino", "*.c",
        "*.py", "*.json", "*.md"
    ]
    
    # Exclude certain directories
    exclude_dirs = {
        '.git', '.github', '.vscode', '__pycache__', 
        'node_modules', 'build', 'venv', '.tox'
    }
    
    # Walk the directory tree
    for root, dirs, filenames in os.walk(project_root):
        # Filter out excluded directories
        dirs[:] = [d for d in dirs if d not in exclude_dirs]
        
        # Skip hidden directories
        dirs[:] = [d for d in dirs if not d.startswith('.')]
        
        for filename in filenames:
            filepath = Path(root) / filename
            rel_path = str(filepath.relative_to(project_root))
            
            # Skip hidden files
            if filename.startswith('.'):
                continue
            
            # Check if file matches patterns
            ext = filepath.suffix.lower()
            if ext in ['.h', '.hpp', '.cpp', '.ino', '.c', '.py', '.json', '.md']:
                files.append(rel_path)
    
    return sorted(files)


def main():
    """Main entry point"""
    import sys
    
    # Determine project root
    script_dir = Path(__file__).parent.parent
    project_root = script_dir.parent
    
    # Allow override via command line
    if len(sys.argv) > 1:
        project_root = Path(sys.argv[1])
    
    # Output directory
    output_dir = project_root / ".openhands" / "librarian" / "files"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Key files to process
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
    
    # Filter to files that exist
    existing_files = []
    for rel_path in key_files:
        full_path = project_root / rel_path
        if full_path.exists():
            existing_files.append(rel_path)
    
    print(f"SGNode File Card Generator v2")
    print(f"=============================")
    print(f"Project: {project_root}")
    print(f"Output: {output_dir}")
    print()
    
    generated_count = 0
    skipped_count = 0
    
    for rel_path in existing_files:
        full_path = project_root / rel_path
        
        try:
            card_content, safe_filename = generate_file_card(project_root, rel_path)
            
            output_path = output_dir / (safe_filename + '.md')
            
            with open(output_path, 'w', encoding='utf-8') as f:
                f.write(card_content)
            
            print(f"  Generated: {safe_filename}.md")
            generated_count += 1
        except Exception as e:
            print(f"  ERROR processing {rel_path}: {e}")
            import traceback
            traceback.print_exc()
            skipped_count += 1
    
    print()
    print(f"Summary:")
    print(f"  Generated: {generated_count}")
    print(f"  Skipped/Error: {skipped_count}")
    
    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main())
