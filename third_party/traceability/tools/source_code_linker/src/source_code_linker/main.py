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
Source Code Linker class for coordinating traceability extraction process.
"""

import argparse
import logging
import sys
from typing import List, Set

from .git_utils import get_github_repo
from .parser import Parser
from .transformer import Transformer
from .writer import Writer

logger = logging.getLogger(__name__)


class SourceCodeLinker:
    """
    Main coordination class that orchestrates the traceability extraction process.
    """

    def __init__(self):
        """Initialize the SourceCodeLinker with its components."""
        self.parser = Parser()
        self.transformer = Transformer()
        self.writer = Writer()

    def setup_logging(self, log_level: str = "INFO") -> None:
        """
        Set up logging configuration.

        Args:
            log_level: Logging level (DEBUG, INFO, WARNING, ERROR)
        """
        numeric_level = getattr(logging, log_level.upper(), None)
        if not isinstance(numeric_level, int):
            raise ValueError(f'Invalid log level: {log_level}')

        logging.basicConfig(
            level=numeric_level,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )

    def parse_arguments(self) -> argparse.Namespace:
        """
        Parse command line arguments.

        Returns:
            Parsed arguments namespace
        """
        parser = argparse.ArgumentParser(
            description="Extract traceability links from source code files"
        )

        # Two ways to provide input files:
        # 1. Positional arguments (for Bazel integration)
        parser.add_argument(
            "inputs",
            nargs='*',
            help="Input files containing lists of source files to process (positional)"
        )

        # 2. Named arguments (for direct usage)
        parser.add_argument(
            "--input-files",
            nargs='+',
            help="Input files containing lists of source files to process (named)"
        )

        parser.add_argument(
            "-o", "--output",
            required=True,
            help="Output lobster JSON file path"
        )

        parser.add_argument(
            "--sources_output",
            help="Output sources text file path (optional)"
        )

        parser.add_argument(
            "-t", "--trace",
            choices=["code", "plantuml_alias_cpp", "plantuml_alias_req", "plantuml"],
            default="code",
            help="Traceability mode: code, plantuml_alias_cpp, plantuml_alias_req, or plantuml"
        )

        parser.add_argument(
            "--tags",
            help="Pipe-separated list of traceability tags to search for"
        )

        parser.add_argument(
            "-u", "--url",
            default="broken_link_g/",
            help="Base GitHub URL"
        )

        parser.add_argument(
            "--log-level",
            choices=["DEBUG", "INFO", "WARNING", "ERROR"],
            default="INFO",
            help="Logging level"
        )

        args = parser.parse_args()

        # Determine input files from either positional or named arguments
        # Priority: --input-files (named) > positional inputs
        if hasattr(args, 'input_files') and args.input_files:
            args.inputs = args.input_files
        else:
            args.inputs = args.inputs or []

        # Convert legacy tags format to list
        if args.tags and isinstance(args.tags, str):
            args.tags = [t.strip() for t in args.tags.split("|") if t.strip()]
        elif not args.tags:
            args.tags = []

        # Auto-generate sources_output if not provided (but different from input)
        if not args.sources_output:
            base_name = args.output.replace('.lobster', '')
            args.sources_output = f"{base_name}_generated_sources.txt"

        return args

    def get_default_tags(self, trace_mode: str) -> List[str]:
        """
        Get default tags for the specified trace mode.

        Args:
            trace_mode: The tracing mode ("code", "plantuml_alias", or "plantuml")

        Returns:
            List of default tags for the mode
        """
        if trace_mode == "code":
            return [
                "# trace:",
                "// trace:",
                "' trace:",
            ]
        else:
            # For "plantuml_alias" and "plantuml" modes, no default tags - rely on trace_tags
            return []

    def load_source_files(self, input_files: List[str]) -> List[str]:
        """
        Load source file paths from input files.

        Args:
            input_files: List of input file paths containing source file lists

        Returns:
            List of source file paths

        Raises:
            IOError: If input files cannot be read
        """
        source_files = []

        for input_file in input_files:
            try:
                with open(input_file) as f:
                    for line in f:
                        source_file = line.strip()
                        if source_file:  # Skip empty lines
                            source_files.append(source_file)
                logger.debug(f"Loaded {len(source_files)} files from {input_file}")
            except IOError as e:
                logger.error(f"Failed to read input file {input_file}: {e}")
                raise

        return source_files

    def process_source_files(
        self,
        source_files: List[str],
        github_base_url: str,
        tags: List[str],
        trace_mode: str
    ) -> List[dict]:
        """
        Process all source files to extract traceability information.

        Args:
            source_files: List of source file paths
            github_base_url: Base GitHub URL
            tags: List of traceability tags
            trace_mode: Tracing mode ("code", "plantuml_alias_cpp", "plantuml_alias_req", or "plantuml")

        Returns:
            List of transformed results from each file
        """
        results = []
        processed_files: Set[str] = set()

        for source_file in source_files:
            # Skip files that have already been processed in this run
            if source_file in processed_files:
                logger.debug(f"Skipping already processed file: {source_file}")
                continue

            processed_files.add(source_file)
            logger.debug(f"Processing file: {source_file}")

            try:
                # Parse the file
                requirement_mapping = self.parser.parse_file(
                    source_file, github_base_url, tags, trace_mode
                )

                # Transform the parsed data
                if requirement_mapping:  # Only process if we found something
                    transformed_result = self.transformer.transform_parsed_data(
                        requirement_mapping, source_file, trace_mode
                    )
                    results.append(transformed_result)
                else:
                    logger.debug(f"No traceability information found in {source_file}")

            except Exception as e:
                logger.warning(f"Error processing file {source_file}: {e}")
                # Continue processing other files

        return results

    def run(self) -> None:
        """
        Main execution method that coordinates the entire process.
        """
        # Parse arguments
        args = self.parse_arguments()

        # Setup logging
        self.setup_logging(args.log_level)

        logger.info("Starting source code linking process")
        logger.info(f"Input files: {args.inputs}")
        logger.info(f"Output: {args.output}")
        logger.info(f"Trace mode: {args.trace}")

        try:
            # Determine tags to use - only use defaults for "code" mode
            if args.tags:
                current_tags = args.tags
            elif args.trace == "code":
                current_tags = self.get_default_tags(args.trace)
            else:
                # For "reqs" and "interface" modes, no fallback to defaults
                current_tags = []
            logger.info(f"Using tags: {current_tags}")

            # Build GitHub base URL
            github_base_url = f"{args.url}{get_github_repo()}"
            logger.info(f"GitHub base URL: {github_base_url}")

            # Load source files
            source_files = self.load_source_files(args.inputs)
            logger.info(f"Processing {len(source_files)} source files")

            # Process source files
            results = self.process_source_files(
                source_files, github_base_url, current_tags, args.trace
            )

            # Merge results
            merged_result = self.transformer.merge_results(results)
            logger.info(f"Generated {len(merged_result['data'])} traceability entries")

            # Write output files
            self.writer.write_output_files(
                merged_result,
                list(source_files),  # Convert set back to list
                args.output,
                args.sources_output
            )

            logger.info("Source code linking process completed successfully")

        except Exception as e:
            logger.error(f"Source code linking process failed: {e}")
            sys.exit(1)


def main():
    """Main entry point for the source code linker."""
    linker = SourceCodeLinker()
    linker.run()


if __name__ == "__main__":
    main()
