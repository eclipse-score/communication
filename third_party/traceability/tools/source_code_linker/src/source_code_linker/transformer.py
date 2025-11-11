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
Transformer class for converting parsed traceability data to lobster format.
"""

import logging
from typing import Dict, List, Any

logger = logging.getLogger(__name__)


class Transformer:
    """
    Transformer class responsible for converting parsed traceability data to the required output format.
    """

    def __init__(self):
        """Initialize the Transformer with lobster templates."""
        self.lobster_code_template = {
            "data": [],
            "generator": "lobster_cpp",
            "schema": "lobster-imp-trace",
            "version": 3
        }

        self.lobster_reqs_template = {
            "data": [],
            "generator": "lobster-trlc",
            "schema": "lobster-req-trace",
            "version": 4
        }

    def transform_to_code_format(
        self,
        requirement_mapping: Dict[str, List[str]],
        source_file: str
    ) -> Dict[str, Any]:
        """
        Transform parsed data to lobster code format.

        Args:
            requirement_mapping: Dictionary mapping requirement IDs to GitHub URLs
            source_file: Path to the source file being processed

        Returns:
            Dictionary in lobster code format
        """
        result = self.lobster_code_template.copy()
        result["data"] = []

        for req_id, links in requirement_mapping.items():
            if not links:
                continue

            link = links[0]  # Take first link if multiple
            # Extract line number from URL (e.g., "file.cpp#L6" -> "6")
            line_number = "0"
            if "#L" in link:
                line_number = link.split("#L")[1]
            # Create unique tag with file and line number
            unique_tag = f"cpp {req_id}:{source_file}:{line_number}"

            codetag = {
                "tag": unique_tag,
                "location": {
                    "kind": "file",
                    "file": link.strip(),
                    "line": 1,
                    "column": 1,
                },
                "name": f"{source_file.strip()}",
                "messages": [],
                "just_up": [],
                "just_down": [],
                "just_global": [],
                "refs": [
                    f"req {req_id}"
                ],
                "language": "cpp",
                "kind": "Function"
            }

            result["data"].append(codetag)

        return result

    def transform_to_requirements_format(
        self,
        requirement_mapping: Dict[str, List[str]],
        source_file: str,
        mode: str = "plantuml"
    ) -> Dict[str, Any]:
        """
        Transform parsed data to lobster requirements format.

        Args:
            requirement_mapping: Dictionary mapping requirement IDs to GitHub URLs
            source_file: Path to the source file being processed
            mode: Processing mode to determine tag formatting

        Returns:
            Dictionary in lobster requirements format
        """
        result = self.lobster_reqs_template.copy()
        result["data"] = []

        for req_id, links in requirement_mapping.items():
            if not links:
                continue

            link = links[0]  # Take first link if multiple

            requirement = {
                "tag": f"req {req_id}",
                "location": {
                    "kind": "file",
                    "file": link.strip(),
                    "line": 1,
                    "column": 1,
                },
                "name": f"{source_file.strip()}",
                "messages": [],
                "just_up": [],
                "just_down": [],
                "just_global": [],
                "framework": "TRLC",
                "kind": "requirement",
                "text": f"{req_id}",
            }

            result["data"].append(requirement)

        return result

    def transform_to_interface_format(
        self,
        requirement_mapping: Dict[str, List[str]],
        source_file: str
    ) -> Dict[str, Any]:
        """
        Transform parsed data to lobster interface requirements format.

        Args:
            requirement_mapping: Dictionary mapping requirement IDs to GitHub URLs
            source_file: Path to the source file being processed

        Returns:
            Dictionary in lobster interface requirements format
        """
        result = self.lobster_reqs_template.copy()
        result["data"] = []

        for req_id, links in requirement_mapping.items():
            if not links:
                continue

            link = links[0]  # Take first link if multiple

            # Extract line number from URL for unique tagging
            line_number = "1"
            if "#L" in link:
                line_number = link.split("#L")[1]

            # Create unique tag with file and line number
            unique_tag = f"req {req_id}:{source_file}:{line_number}"

            interface_req = {
                "tag": unique_tag,
                "location": {
                    "kind": "file",
                    "file": link.strip(),
                    "line": 1,
                    "column": 1,
                },
                "name": f"{source_file.strip()}",
                "messages": [],
                "just_up": [],
                "just_down": [],
                "just_global": [],
                "framework": "TRLC",
                "kind": "requirement",
                "text": f"{req_id}",
            }

            result["data"].append(interface_req)

        return result

    def transform_parsed_data(
        self,
        requirement_mapping: Dict[str, List[str]],
        source_file: str,
        mode: str
    ) -> Dict[str, Any]:
        """
        Transform parsed data to the specified output format.

        Args:
            requirement_mapping: Dictionary mapping requirement IDs to GitHub URLs
            source_file: Path to the source file being processed
            mode: Output mode ("code", "plantuml_alias_cpp", "plantuml_alias_req", or "plantuml")

        Returns:
            Dictionary in the specified lobster format
        """
        if mode == "plantuml_alias_cpp":
            return self.transform_to_code_format(requirement_mapping, source_file)
        elif mode == "plantuml_alias_req":
            return self.transform_to_requirements_format(requirement_mapping, source_file, mode)
        elif mode == "plantuml":
            return self.transform_to_requirements_format(requirement_mapping, source_file, mode)
        else:  # mode == "code" (default)
            return self.transform_to_code_format(requirement_mapping, source_file)

    def merge_results(self, results: List[Dict[str, Any]]) -> Dict[str, Any]:
        """
        Merge multiple lobster results into a single result.

        Args:
            results: List of lobster result dictionaries

        Returns:
            Merged lobster result dictionary
        """
        if not results:
            return self.lobster_code_template.copy()

        # Use the first result as the base
        merged = {
            "data": [],
            "generator": results[0].get("generator", "lobster_cpp"),
            "schema": results[0].get("schema", "lobster-imp-trace"),
            "version": results[0].get("version", 3)
        }

        # Collect all data entries
        all_data = []
        for result in results:
            all_data.extend(result.get("data", []))

        # Deduplicate entries that are truly identical (same tag, same file, same location)
        # but allow multiple entries for the same tag at different locations
        seen_entries = set()
        deduped_data = []

        for item in all_data:
            # Create a unique key based on tag, file, and location (line number from URL)
            tag = item.get("tag", "")
            file_url = item.get("location", {}).get("file", "")
            # The line number is in the URL as #L<number>
            entry_key = (tag, file_url)

            if entry_key not in seen_entries:
                seen_entries.add(entry_key)
                deduped_data.append(item)
            else:
                logger.debug(f"Removing truly duplicate entry: {tag} at {file_url}")

        merged["data"] = deduped_data
        return merged
