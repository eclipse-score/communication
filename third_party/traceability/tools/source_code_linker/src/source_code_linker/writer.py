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
Writer class for generating output files from transformed traceability data.
"""

import json
import logging
from typing import Dict, Any

logger = logging.getLogger(__name__)


class Writer:
    """
    Writer class responsible for generating all target files from transformed data.
    """

    def __init__(self):
        """Initialize the Writer."""
        pass

    def write_lobster_file(self, data: Dict[str, Any], output_path: str) -> None:
        """
        Write lobster data to a JSON file.

        Args:
            data: Dictionary containing lobster data
            output_path: Path where the file should be written

        Raises:
            IOError: If the file cannot be written
        """
        try:
            with open(output_path, "w") as f:
                json.dump(data, f, indent=2)
            logger.info(f"Successfully wrote lobster file: {output_path}")
        except IOError as e:
            logger.error(f"Failed to write lobster file {output_path}: {e}")
            raise

    def write_sources_file(self, source_files: list, output_path: str) -> None:
        """
        Write list of source files to a text file.

        Args:
            source_files: List of source file paths
            output_path: Path where the file should be written

        Raises:
            IOError: If the file cannot be written
        """
        try:
            with open(output_path, "w") as f:
                for source_file in source_files:
                    f.write(f"{source_file}\n")
            logger.info(f"Successfully wrote sources file: {output_path}")
        except IOError as e:
            logger.error(f"Failed to write sources file {output_path}: {e}")
            raise

    def validate_lobster_data(self, data: Dict[str, Any]) -> bool:
        """
        Validate lobster data structure.

        Args:
            data: Dictionary containing lobster data

        Returns:
            True if data is valid, False otherwise
        """
        required_fields = ["data", "generator", "schema", "version"]

        # Check required top-level fields
        for field in required_fields:
            if field not in data:
                logger.error(f"Missing required field in lobster data: {field}")
                return False

        # Check data is a list
        if not isinstance(data["data"], list):
            logger.error("Lobster data 'data' field must be a list")
            return False

        # Validate individual data entries
        for i, entry in enumerate(data["data"]):
            if not isinstance(entry, dict):
                logger.error(f"Lobster data entry {i} must be a dictionary")
                return False

            # Check for required entry fields
            required_entry_fields = ["tag", "location"]
            for field in required_entry_fields:
                if field not in entry:
                    logger.warning(f"Entry {i} missing recommended field: {field}")

        return True

    def format_lobster_data(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """
        Format and clean lobster data before writing.

        Args:
            data: Dictionary containing lobster data

        Returns:
            Formatted lobster data
        """
        # Make a copy to avoid modifying the original
        formatted_data = data.copy()

        # Ensure data is sorted by tag for consistent output
        if "data" in formatted_data and isinstance(formatted_data["data"], list):
            formatted_data["data"] = sorted(
                formatted_data["data"],
                key=lambda x: x.get("tag", "")
            )

        return formatted_data

    def write_output_files(
        self,
        lobster_data: Dict[str, Any],
        source_files: list,
        lobster_output_path: str,
        sources_output_path: str
    ) -> None:
        """
        Write all output files.

        Args:
            lobster_data: Dictionary containing lobster data
            source_files: List of processed source files
            lobster_output_path: Path for lobster JSON output
            sources_output_path: Path for sources text output

        Raises:
            ValueError: If data validation fails
            IOError: If files cannot be written
        """
        # Validate data before writing
        if not self.validate_lobster_data(lobster_data):
            raise ValueError("Invalid lobster data structure")

        # Format data
        formatted_data = self.format_lobster_data(lobster_data)

        # Write lobster file
        self.write_lobster_file(formatted_data, lobster_output_path)

        # Write sources file (only if it's a different path than the inputs)
        if sources_output_path:
            # Check if sources_output_path is one of the input files
            try:
                # Try to determine if this is an input file by checking if we can read from it
                with open(sources_output_path, 'r') as f:
                    content = f.read().strip()
                    # If file contains source file paths, it's likely an input file
                    if content and not content.startswith('[') and not content.startswith('{'):
                        logger.debug(f"Skipping sources file write - appears to be input file: {sources_output_path}")
                        return
            except (IOError, PermissionError):
                pass

            # Write the sources file
            self.write_sources_file(source_files, sources_output_path)

        logger.info(f"Successfully wrote {len(formatted_data['data'])} entries to output files")
