# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

"""Extract API items from Doxygen XML and generate RST documentation.

This script parses Doxygen api.xml output (pre-filtered @api tagged items)
and generates corresponding RST files for Sphinx documentation.
"""

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Optional


class APITagExtractor: # pylint: disable=too-few-public-methods
    """Extracts API-tagged items from Doxygen XML output."""

    def __init__(self, xml_file_path: str):
        """Initialize the API extractor.

        Args:
            xml_file_path: Path to the Doxygen api.xml file
        """
        self.xml_file_path = xml_file_path
        self.xml_dir = str(Path(xml_file_path).parent)
        self.root = None
        self._load_xml()
        self._member_kind_cache: Dict[str, str] = {}
        self._function_overloads: Dict[str, int] = {}

    def _load_xml(self) -> None:
        """Load and parse the XML file."""
        try:
            tree = ET.parse(self.xml_file_path)
            self.root = tree.getroot()
        except ET.ParseError as e:
            print(
                f"Error parsing XML file {self.xml_file_path}: {e}",
                file=sys.stderr
            )
            sys.exit(1)
        except FileNotFoundError:
            print(f"XML file not found: {self.xml_file_path}", file=sys.stderr)
            sys.exit(1)

    def extract_api_items(self) -> Dict[str, List[Dict[str, str]]]:
        """Extract all API-tagged items from the XML.

        Returns:
            Dictionary with categorized API items, each containing name and id
        """
        api_items = {
            'namespaces': [],
            'classes': [],
            'members': []
        }

        # Verify this is an api.xml file with pre-filtered @api tagged items
        if not self._is_api_reference_file():
            print(
                f"Error: Expected api.xml file with @api tagged items, "
                f"got {self.xml_file_path}",
                file=sys.stderr
            )
            sys.exit(1)

        self._extract_from_api_references(api_items)
        return api_items

    def _extract_member_signature(self, term_elem: ET.Element) -> str:
        """Extract member signature from term element.

        Args:
            term_elem: The <term> element containing member signature

        Returns:
            Full signature text with parameters if present
        """
        # Get all text content including signature parts
        signature_parts = []
        for text in term_elem.itertext():
            text = text.strip()
            if text:
                signature_parts.append(text)

        # Join and clean up the signature
        full_signature = ' '.join(signature_parts)
        # Remove extra whitespace
        full_signature = ' '.join(full_signature.split())
        return full_signature

    def _get_member_kind_from_xml(self, refid: str) -> Optional[str]:
        """Get the kind of a member by parsing its XML file.

        Args:
            refid: The reference ID from api.xml
                   (e.g., "namespacebmw_1_1mw_1_1com_1ad...")

        Returns:
            The kind attribute (typedef, function, enum, variable)
            or None if not found
        """
        if refid in self._member_kind_cache:
            return self._member_kind_cache[refid]

        # Extract the compound file name from refid
        # For namespace members: "namespacebmw_1_1mw_1_1com_1a..."
        #   -> "namespacebmw_1_1mw_1_1com.xml"
        # For class members:
        #   "classbmw_1_1mw_1_1com_1_1impl_1_1FindServiceHandle_1a..."
        #   -> "classbmw_1_1mw_1_1com_1_1impl_1_1FindServiceHandle.xml"

        # Split on the last occurrence of _1 followed by a lowercase letter
        match = re.match(
            r'((?:namespace|class|struct)[^_]+(?:_1_1[^_]+)*?)_1[a-f0-9]',
            refid
        )
        if match:
            compound_name = match.group(1)
            xml_file = Path(self.xml_dir) / f"{compound_name}.xml"

            try:
                tree = ET.parse(str(xml_file))
                root = tree.getroot()

                # Find the memberdef with this id
                memberdef = root.find(f'.//memberdef[@id="{refid}"]')
                if memberdef is not None:
                    kind = memberdef.get('kind', '')
                    self._member_kind_cache[refid] = kind
                    return kind
            except (FileNotFoundError, ET.ParseError):
                pass

        self._member_kind_cache[refid] = None
        return None

    def _is_api_reference_file(self) -> bool:
        """Check if this XML file is an api.xml reference file.

        The api.xml file has a special structure:
        - Root element: <doxygen>
        - Contains: <compounddef kind="page" id="api">
        - Title: "Public API"

        Returns:
            True if this is an api.xml reference file
        """
        # Check if root has a compounddef with kind="page" and id="api"
        api_compound = self.root.find(
            './/compounddef[@kind="page"][@id="api"]'
        )
        return api_compound is not None

    def _extract_from_api_references(
            self, api_items: Dict[str, List[Dict[str, str]]]
    ) -> None:
        """Extract API items from api.xml reference file.

        The api.xml file structure contains:
        - <varlistentry> with <term> describing the item type
          (Namespace/Class/Member)
        - <ref> element with refid and kindref attributes containing
          the identifier
        - Full signature information in the term text

        Note: Doxygen marks everything as "Member" except Namespace and Class.
        This includes class methods, standalone functions, typedefs, enums,
        variables, etc.

        Args:
            api_items: Dictionary to store extracted items
        """
        # Track function base names and their overload count
        function_overloads = {}

        # Find all varlistentry elements in the api.xml file
        for varlistentry in self.root.findall('.//varlistentry'):
            term_elem = varlistentry.find('term')
            if term_elem is None:
                continue

            # Get the term text to determine item type
            term_text = ''.join(term_elem.itertext()).strip()

            # Find the ref element within the term
            ref_elem = term_elem.find('.//ref')
            if ref_elem is None:
                continue

            refid = ref_elem.get('refid', '')
            ref_text = ref_elem.text or ''

            if not refid or not ref_text:
                continue

            # Determine category based on term prefix (only 3 types in api.xml)
            if term_text.startswith('Namespace '):
                # Extract namespace name
                item_data = {'name': ref_text.strip(), 'id': refid}
                api_items['namespaces'].append(item_data)

            elif term_text.startswith('Class '):
                # Extract class name
                item_data = {'name': ref_text.strip(), 'id': refid}
                api_items['classes'].append(item_data)

            elif term_text.startswith('Member '):
                # Extract member with full signature if present
                # This includes: methods, functions, typedefs, enums,
                # variables, etc.
                signature = self._extract_member_signature(term_elem)
                # Remove "Member " prefix to get the actual signature
                if signature.startswith('Member '):
                    signature = signature[7:].strip()

                # Get the actual kind from the XML file
                member_kind = self._get_member_kind_from_xml(refid)

                # Track function overloads
                if member_kind == 'function' or '(' in signature:
                    base_name = signature.split('(', maxsplit=1)[0].strip()
                    function_overloads[base_name] = (
                        function_overloads.get(base_name, 0) + 1
                    )

                item_data = {
                    'name': signature,
                    'id': refid,
                    'kind': member_kind
                }
                api_items['members'].append(item_data)

        # Store overload information for RST generation
        self._function_overloads = function_overloads


class RSTGenerator: # pylint: disable=too-few-public-methods
    """Generates RST documentation files from extracted API items."""

    def __init__(self, project_name: str, output_dir: str):
        """Initialize the RST generator.

        Args:
            project_name: Name of the project for documentation
            output_dir: Directory to write RST files
        """
        self.project_name = project_name
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def generate_rst_files(
            self, api_items: Dict[str, List[Dict[str, str]]]
    ) -> List[str]:
        """Generate separate RST files for each category.

        Args:
            api_items: Dictionary of categorized API items

        Returns:
            List of generated RST file paths
        """
        generated_files = []

        # Generate main index file
        index_file = self._generate_index_file(api_items)
        generated_files.append(index_file)

        # Generate category-specific files
        for category, items in api_items.items():
            if items:  # Only generate files for non-empty categories
                rst_file = self._generate_category_file(category, items)
                generated_files.append(rst_file)

        return generated_files

    def _generate_index_file(
            self, api_items: Dict[str, List[Dict[str, str]]]
    ) -> str:
        """Generate main API index RST file.

        Args:
            api_items: Dictionary of categorized API items

        Returns:
            Path to generated index file
        """
        content = f"""Public API Reference
=============

This document contains all public API items tagged with @api for \
{self.project_name}.

.. toctree::
   :maxdepth: 2
   :caption: API Categories:

"""

        # Add toctree entries for non-empty categories
        for category, items in api_items.items():
            if items:
                content += f"   api_{category}\n"

        content += """
Overview
--------

The API documentation is automatically generated from source code items \
marked with the ``@api`` tag.
Each section below contains detailed documentation for the corresponding \
category.

"""

        # Add summary statistics
        total_items = sum(len(items) for items in api_items.values())
        content += f"**Total API Items: {total_items}**\n\n"

        for category, items in api_items.items():
            if items:
                content += f"- {category.title()}: {len(items)} items\n"

        index_path = self.output_dir / "api_index.rst"
        with open(index_path, 'w', encoding='utf-8') as f:
            f.write(content)

        return str(index_path)

    def _generate_category_file(
            self, category: str, items: List[Dict[str, str]]
    ) -> str:
        """Generate RST file for a specific category.

        Args:
            category: Category name (classes, functions, etc.)
            items: List of items in this category

        Returns:
            Path to generated category file
        """
        title = category.title().rstrip('s')  # Remove trailing 's'
        content = f"""{title} Reference
{'=' * (len(title) + 11)}

This section contains all {category} tagged with @api.

"""

        # Sort items by name for consistent output
        sorted_items = sorted(items, key=lambda x: x['name'])

        for item in sorted_items:
            name = item['name']
            kind = item.get('kind')  # Get the kind if available
            content += self._generate_item_documentation(category, name, kind)

        file_path = self.output_dir / f"api_{category}.rst"
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)

        return str(file_path)

    def _simplify_signature(self, signature: str) -> str: # pylint: disable=too-many-locals,too-many-branches
        """Simplify a function signature for Breathe.

        Converts: "ClassName::method (const Type &param) noexcept = default"
        To: "ClassName::method(const Type &)"

        Args:
            signature: Full function signature with parameter names
                       and qualifiers

        Returns:
            Simplified signature with just parameter types
        """
        # Extract base name and parameters
        if '(' not in signature:
            return signature

        base_name = signature.split('(', maxsplit=1)[0].strip()

        # Extract everything between first ( and last )
        # Handle nested parentheses in parameter types
        paren_start = signature.index('(')
        paren_count = 0
        paren_end = paren_start

        for i in range(paren_start, len(signature)):
            if signature[i] == '(':
                paren_count += 1
            elif signature[i] == ')':
                paren_count -= 1
                if paren_count == 0:
                    paren_end = i
                    break

        params_str = signature[paren_start+1:paren_end]

        # Simplify parameters: remove parameter names, keep types
        # This is a simplified approach - for complex cases,
        # the full signature might be needed
        simplified_params = []

        # Split by comma, but respect angle brackets and parentheses
        param_parts = []
        current_param = ""
        depth = 0

        for char in params_str:
            if char in '<(':
                depth += 1
            elif char in '>)':
                depth -= 1
            elif char == ',' and depth == 0:
                param_parts.append(current_param.strip())
                current_param = ""
                continue
            current_param += char

        if current_param.strip():
            param_parts.append(current_param.strip())

        # For each parameter, extract just the type (remove parameter name)
        for param in param_parts:
            if not param:
                continue

            # Remove trailing parameter name
            # Pattern: "const Type &name" -> "const Type &"
            # Pattern: "Type &&name" -> "Type &&"
            # Keep the type and reference/pointer qualifiers

            # Simple heuristic: parameter name is the last word after
            # the last type qualifier
            # This is imperfect but works for most cases
            param_clean = param.strip()

            # Remove default values (everything after =)
            if '=' in param_clean:
                param_clean = param_clean.split('=', maxsplit=1)[0].strip()

            # For complex types, just use the parameter as-is
            # Breathe is quite flexible with parameter matching
            simplified_params.append(param_clean)

        # Reconstruct the signature
        simplified = f"{base_name}({', '.join(simplified_params)})"

        # Check for const qualifier after the closing parenthesis
        # Look for "const" keyword after the parameters
        # but before other qualifiers
        remainder = signature[paren_end+1:].strip()
        if remainder.startswith('const'):
            # Add const to the signature, but remove other qualifiers
            # like noexcept, override, final, =default
            simplified += " const"

        return simplified

    def _generate_item_documentation( # pylint: disable=too-many-return-statements
            self, category: str, name: str, kind: Optional[str] = None
    ) -> str:
        """Generate RST directive for a single item.

        Args:
            category: Category of the item (namespaces, classes, members)
            name: Name or signature of the item
            kind: The Doxygen kind (typedef, function, enum, variable, etc.)

        Returns:
            RST content for the item
        """
        if category == 'namespaces':
            return f"""
.. doxygennamespace:: {name}
   :content-only:

"""
        if category == 'classes':
            return f"""
.. doxygenclass:: {name}
   :members:
   :undoc-members:

"""
        if category == 'members':
            # For callable items, we may need the full signature for
            # overloaded functions
            # For non-callable items (typedefs, enums), use just the name

            # Extract base name for categorization
            base_name = (name.split('(', maxsplit=1)[0].strip()
                         if '(' in name else name)

            # Use the kind to determine the appropriate directive
            if kind in ('function', 'friend'):
                # For functions and friend functions, always use signature
                # if available for better precision
                # This helps Breathe resolve overloaded functions,
                # templates, operators, etc.
                if '(' in name:
                    # Has signature: simplify and use it
                    func_signature = self._simplify_signature(name)
                    return f"""
.. doxygenfunction:: {func_signature}

"""
                # No signature (unusual): use base name
                return f"""
.. doxygenfunction:: {base_name}

"""
            if kind == 'typedef':
                return f"""
.. doxygentypedef:: {base_name}

"""
            if kind == 'enum':
                return f"""
.. doxygenenum:: {base_name}

"""
            if kind == 'variable':
                return f"""
.. doxygenvariable:: {base_name}

"""
            # Fallback: Use function directive if it has parentheses,
            # otherwise try typedef
            if '(' in name:
                return f"""
.. doxygenfunction:: {base_name}

"""
            # For unknown types without parentheses, try typedef
            # as it's most common
            return f"""
.. doxygentypedef:: {base_name}

"""
        return f"""
.. note:: {name}

"""


def parse_arguments() -> argparse.Namespace:
    """Parse command line arguments.

    Returns:
        Parsed command line arguments
    """
    parser = argparse.ArgumentParser(
        description='Extract API items from Doxygen XML and generate RST files',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    # Support both positional and flag-based arguments for flexibility
    parser.add_argument(
        'xml_file',
        nargs='?',
        help='Path to Doxygen api.xml file'
    )
    parser.add_argument(
        'output_dir',
        nargs='?',
        help='Output directory for generated RST files'
    )
    parser.add_argument(
        '--xml-file',
        dest='xml_file_flag',
        help='Path to Doxygen api.xml file (alternative to positional)'
    )
    parser.add_argument(
        '--output-dir',
        dest='output_dir_flag',
        help='Output directory for generated RST files (alternative to positional)'
    )
    parser.add_argument(
        '--project-name', default='Project',
        help='Project name for documentation (default: Project)'
    )
    parser.add_argument(
        '--max-items', type=int, default=1000,
        help='Maximum number of items per category (default: 1000)'
    )
    parser.add_argument(
        '--verbose', '-v', action='store_true',
        help='Enable verbose output'
    )

    args = parser.parse_args()

    # Resolve xml_file: prefer flag, fallback to positional
    if args.xml_file_flag:
        args.xml_file = args.xml_file_flag
    elif not args.xml_file:
        parser.error('xml_file is required (either positional or --xml-file)')

    # Resolve output_dir: prefer flag, fallback to positional
    if args.output_dir_flag:
        args.output_dir = args.output_dir_flag
    elif not args.output_dir:
        parser.error('output_dir is required (either positional or --output-dir)')

    return args


def main() -> None:
    """Main entry point."""
    args = parse_arguments()

    if args.verbose:
        print(f"Processing XML file: {args.xml_file}")
        print(f"Output directory: {args.output_dir}")
        print(f"Project name: {args.project_name}")

    # Extract API items
    extractor = APITagExtractor(args.xml_file)
    api_items = extractor.extract_api_items()

    # Apply performance limits if specified
    for category, items in api_items.items():
        if len(items) > args.max_items:
            if args.verbose:
                print(
                    f"Warning: Limiting {category} from {len(items)} "
                    f"to {args.max_items} items"
                )
            api_items[category] = items[:args.max_items]

    # Generate RST files
    generator = RSTGenerator(args.project_name, args.output_dir)
    generated_files = generator.generate_rst_files(api_items)

    # Print summary
    total_items = sum(len(items) for items in api_items.values())
    print(
        f"Generated {len(generated_files)} RST files with "
        f"{total_items} API items"
    )

    if args.verbose:
        for category, items in api_items.items():
            if items:
                print(f"  {category}: {len(items)} items")
        print("Generated files:")
        for file_path in generated_files:
            print(f"  {file_path}")


if __name__ == '__main__':
    main()
