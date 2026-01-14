#!/usr/bin/env python3
"""
Convert JSON with kebab-case keys to FlatBuffer-compatible JSON with snake_case keys.

This script recursively processes JSON objects and converts all kebab-case keys
(e.g., "asil-level") to snake_case (e.g., "asil_level") to match FlatBuffer field
naming conventions.
"""

import json
import sys
import re
from pathlib import Path
from typing import Any


def to_snake_case(text: str) -> str:
    """
    Convert kebab-case or camelCase string to snake_case
    
    Args:
        text: String in kebab-case or camelCase format
    
    Returns:
        String in snake_case format
    """
    result = text.replace('-', '_')
    
    # Preserve consecutive uppercase letters (like ID, QM, B)
    # Insert underscore before uppercase letter that follows lowercase letter
    result = re.sub(r'([a-z])([A-Z])', r'\1_\2', result)
    # Insert underscore before uppercase letter that is followed by lowercase
    result = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1_\2', result)

    return result.lower()


def convert_json(obj: Any) -> Any:
    """
    Recursively converts all dictionary keys to snake_case and applies custom conversion
    for specific string values which are implemented as enum in FlatBuffers.
    
    Args:
        obj: JSON object (dict, list, or primitive type)
    
    Returns:
        Converted JSON object
    """
    if isinstance(obj, dict):
        converted = {}
        for key, value in obj.items():
            new_key = to_snake_case(key)
            new_value = convert_enum_value(new_key, value)
            converted[new_key] = convert_json(new_value)
        return converted
    elif isinstance(obj, list):
        return [convert_json(item) for item in obj]
    else:
        return obj


def convert_enum_value(key: str, value: Any) -> Any:
    """
    Convert communication JSON enum string values to FlatBuffer friendly format.
    
    Args:
        key: The field name
        value: The field value
    
    Returns:
        Converted value
    """
    if not isinstance(value, str):
        return value
    
    # Binding type: "SOME/IP" -> "SOME_IP", "SHM" -> "SHM"
    if key == 'binding':
        return value.replace('/', '_')
    
    # Permission checks: "file-permissions-on-empty" -> "FILE_PERMISSIONS_ON_EMPTY", "strict" -> "STRICT"
    if key == 'permission_checks':
        return value.replace('-', '_').upper()
        
    return value


def main():
    """Main entry point for the script."""
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.json> <output.json>", file=sys.stderr)
        sys.exit(1)
    
    input_file = Path(sys.argv[1])
    output_file = Path(sys.argv[2])
    
    # Validate input file exists
    if not input_file.exists():
        print(f"Error: Input file '{input_file}' does not exist", file=sys.stderr)
        sys.exit(1)
    
    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            data = json.load(f)

        converted_data = convert_json(data)

        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(converted_data, f, indent=4, ensure_ascii=False)
            f.write('\n')
        print(f"Successfully converted {input_file} -> {output_file}")
        
    except json.JSONDecodeError as e:
        print(f"Error: Failed to parse JSON from '{input_file}': {e}", file=sys.stderr)
        sys.exit(1)
    except IOError as e:
        print(f"Error: I/O error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
