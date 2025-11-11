# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

"""
Parser class for extracting traceability information from source files.
"""

import collections
import logging
import re
from typing import Dict, List, Optional, Callable

from .git_utils import get_git_hash

logger = logging.getLogger(__name__)


class Parser:
    """
    Parser class responsible for extracting traceability tags and information from source files.
    """

    def __init__(self, git_hash_func: Optional[Callable[[str], str]] = None):
        """
        Initialize the Parser.

        Args:
            git_hash_func: Optional git hash function for testing purposes
        """
        self.git_hash_func = git_hash_func or get_git_hash
        self.standard_tags = [
            "# trace:",
            "// trace:",
            "' trace:",
        ]

    def extract_id_from_line_standard(self, line: str, tags: List[str]) -> Optional[str]:
        """
        Parse a single line to extract the ID from standard source files.

        Parameters
        ----------
        line : str
            A single line of text containing a traceability tag.
        tags : List[str]
            A list of tag strings to search for.

        Returns
        -------
        Optional[str]
            The extracted ID if found, None otherwise.
        """
        for tag in tags:
            if tag in line:
                parts = line.split(tag)
                if len(parts) > 1:
                    # Extract the ID after the tag
                    id_part = parts[1].strip()
                    # Remove any trailing comments or whitespace
                    return id_part.split()[0] if id_part else None
        return None

    def extract_id_from_plantuml_alias(self, line: str, nodes: List[str]) -> Optional[str]:
        """
        Parse a single line to extract the ID from PlantUML alias nodes (e.g., $TopEvent, $BasicEvent).

        Parameters
        ----------
        line : str
            A single line of text containing a PlantUML alias node.
        nodes : List[str]
            A list of alias node strings to search for.

        Returns
        -------
        Optional[str]
            The extracted ID if found, None otherwise.
        """
        # Step 1: Clean the line of \n, and \\n
        cleaned_line = line.replace('\n', '').replace('\\n', '')

        # Step 2: Remove all single and double quotes
        cleaned_line = cleaned_line.replace('"', '').replace("'", '').replace("{",'').strip()

        # Step 3: Search for nodes and capture the ID
        for node in nodes:
            if cleaned_line.startswith(node):
                # Scan for Macro
                if node.startswith('$'):
                    parts = cleaned_line.split(',')
                    if len(parts) >= 2:
                        return parts[1].strip().rstrip(')')
                else: # scan for normal plantuml element
                    # Remove the identifier from the start of the line
                    parts = cleaned_line[len(node):].strip().split()

                    # If there are at least 3 parts and the second-to-last is 'as', return the last part
                    if len(parts) >= 3 and parts[-2] == 'as':
                        return parts[-1], node

                    # If there's only one part after the identifier, return it
                    if len(parts) == 1:
                        return parts[0]

        return None

    def extract_id_from_plantuml_element(self, line: str, node: str) -> Optional[str]:
        """
        Extract ID from general PlantUML elements (component, class, interface, etc.).

        Args:
            line (str): Line to parse
            node (str): Node type to search for (e.g., "component", "class", "interface", etc.)

        Returns:
            Optional[str]: The extracted ID if found, None otherwise.
        """
        cleaned_line = line.replace('\n', '').replace('\\n', '').strip()

        # Escape the node name for safe regex usage
        escaped_node = re.escape(node)

        # Special handling for interface methods (e.g., +GetNumber(), +SetNumber())
        if node == "interface":
            # Look for method pattern: +method_name()
            method_pattern = r'^\s*\+(\w+)\('
            match = re.match(method_pattern, cleaned_line)
            if match:
                method_name = match.group(1)
                return method_name

        # Pattern 1: <tag> "SampleLibrary" as SL
        quoted_with_alias = re.search(rf'^\s*{escaped_node}\s+"([^"]+)"\s+as\s+(\w+)', cleaned_line)
        if quoted_with_alias:
            return quoted_with_alias.group(2)  # Return alias

        # Pattern 2: <tag> "SampleLibrary"
        quoted_no_alias = re.search(rf'^\s*{escaped_node}\s+"([^"]+)"', cleaned_line)
        if quoted_no_alias:
            return quoted_no_alias.group(1)  # Return name

        # Pattern 3: <tag> SampleLibrary
        simple_no_alias = re.search(rf'^\s*{escaped_node}\s+(\w+)', cleaned_line)
        if simple_no_alias:
            return simple_no_alias.group(1)  # Return name

        return None

    def parse_standard_file(
        self,
        source_file: str,
        github_base_url: str,
        tags: List[str]
    ) -> Dict[str, List[str]]:
        """
        Extract tags from standard source files (non-PlantUML files).

        Args:
            source_file (str): path to source file that should be parsed.
            github_base_url (str): base URL for GitHub repository.
            tags (List[str]): list of tag strings to search for.

        Returns:
            Dict[str, List[str]]: mapping of requirement IDs to GitHub URLs.
        """
        requirement_mapping: Dict[str, List[str]] = collections.defaultdict(list)

        with open(source_file) as f:
            hash = self.git_hash_func(source_file)
            for line_number, line in enumerate(f):
                line_number = line_number + 1
                result = self.extract_id_from_line_standard(line, tags)
                if result is None:
                    continue

                req_id = result
                if req_id:
                    link = f"{github_base_url}/blob/{hash}/{source_file}#L{line_number}"
                    requirement_mapping[req_id].append(link)

        return requirement_mapping

    def parse_plantuml_file(
        self,
        source_file: str,
        github_base_url: str,
        nodes: List[str]
    ) -> Dict[str, List[str]]:
        """
        Extract tags from PlantUML files (.puml files).

        Supports parsing of PlantUML elements based on the provided nodes list.

        Args:
            source_file (str): path to PlantUML file that should be parsed.
            github_base_url (str): base URL for GitHub repository.
            nodes (List[str]): list of PlantUML node strings to search for.

        Returns:
            Dict[str, List[str]]: mapping of requirement IDs to GitHub URLs.
        """
        requirement_mapping: Dict[str, List[str]] = collections.defaultdict(list)

        with open(source_file) as f:
            hash = self.git_hash_func(source_file)
            current_interface = None  # Track current interface for method prefixing

            for line_number, line in enumerate(f):
                line_number = line_number + 1
                cleaned_line = line.replace('\n', '').replace('\\n', '').strip()

                # Track interface context for proper method prefixing
                if "interface" in nodes:
                    # Check if this line defines an interface
                    interface_name = self.extract_id_from_plantuml_element(line, "interface")
                    if interface_name and not cleaned_line.strip().startswith('+'):
                        # This is an interface definition, not a method
                        current_interface = interface_name
                    elif cleaned_line.strip() == '}':
                        # End of interface block
                        current_interface = None

                # Parse each line for the specified tags/nodes
                for node in nodes:
                    req_id = self.extract_plantuml_element(line, node)
                    if req_id:
                        # For interface methods, prefix with interface name
                        if node == "interface" and current_interface and cleaned_line.strip().startswith('+'):
                            req_id = f"{current_interface}.{req_id}"

                        link = f"{github_base_url}/blob/{hash}/{source_file}#L{line_number}"
                        requirement_mapping[req_id].append(link)

        return requirement_mapping

    def extract_plantuml_element(self, line: str, node: str) -> Optional[str]:
        """
        Extract PlantUML element based on the node type.

        Args:
            line (str): Line to parse
            node (str): Node type to search for (e.g., "component", "class", "interface", etc.)

        Returns:
            Optional[str]: The extracted ID if found, None otherwise.
        """
        # For plantuml_alias mode (nodes starting with $), use the alias extraction logic
        if node.startswith('$'):
            return self.extract_id_from_plantuml_alias(line, [node])
        else:
            # For general plantuml mode, use the element extraction logic
            return self.extract_id_from_plantuml_element(line, node)

    def parse_file(
        self,
        source_file: str,
        github_base_url: str,
        tags: List[str],
        mode: str = "code",
        nodes: Optional[List[str]] = None
    ) -> Dict[str, List[str]]:
        """
        Dispatch to appropriate parser based on file extension and mode.

        Args:
            source_file (str): path to source file that should be parsed.
            github_base_url (str): base URL for GitHub repository.
            tags (List[str]): list of tag strings to search for.
            mode (str): tracing mode ("code", "plantuml_alias_cpp", "plantuml_alias_req", or "plantuml")
            nodes (Optional[List[str]]): list of PlantUML node strings to search for (for .puml files).

        Returns:
            Dict[str, List[str]]: mapping of requirement IDs to GitHub URLs.
        """
        if source_file.endswith('.puml'):
            if mode == "plantuml_alias_cpp" or mode == "plantuml_alias_req" or mode == "plantuml":
                # For plantuml modes: process all tags without filtering
                if tags:
                    return self.parse_plantuml_file(source_file, github_base_url, tags)
                else:
                    return {}
            else:
                # For code mode: use standard file parsing
                return self.parse_standard_file(source_file, github_base_url, tags)
        else:
            # For non-PlantUML files, use standard parsing
            return self.parse_standard_file(source_file, github_base_url, tags)
