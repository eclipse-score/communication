#!/usr/bin/env python3
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
"""Generate HTML coverage reports from LCOV data using gcovr.

Converts LCOV trace data to gcovr's JSON format (v0.14), then invokes gcovr
to produce syntax-highlighted HTML reports with full source code annotation.

The output uses gcovr's native HTML format with --html-details, producing:
  <output-dir>/
    coverage_details.html          <- Index page
    coverage_details.<name>.<md5>.html  <- Per-source annotated pages
    style.css, *.js                <- Assets

Usage:
    python lcov_to_html.py --lcov <lcov.dat> --output-dir <dir> \
        [--source-root <path>] [--filter-regexes <file>]
"""

import argparse
import json
import os
import re
import sys
import tempfile
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


def parse_lcov(lcov_path: Path) -> List[Dict[str, Any]]:
    """Parse an LCOV trace file into gcovr JSON v0.14 file entries.

    Returns a list of dicts, each representing one file in the gcovr JSON
    format with keys: "file", "lines", "functions".
    """
    files: List[Dict[str, Any]] = []
    current_file: Optional[str] = None
    current_lines: Dict[int, Dict[str, Any]] = {}
    current_branches: Dict[int, List[Dict[str, Any]]] = {}
    fn_lines: Dict[str, int] = {}
    fn_counts: Dict[str, int] = {}

    with open(lcov_path, encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()

            if line.startswith("SF:"):
                if current_file is not None:
                    files.append(_build_file_entry(
                        current_file, current_lines, current_branches,
                        fn_lines, fn_counts))
                current_file = line[3:]
                current_lines = {}
                current_branches = {}
                fn_lines = {}
                fn_counts = {}

            elif current_file is None:
                continue

            elif line.startswith("DA:"):
                parts = line[3:].split(",")
                if len(parts) >= 2:
                    lineno = int(parts[0])
                    count = max(0, int(parts[1]))
                    current_lines[lineno] = {"line_number": lineno, "count": count}

            elif line.startswith("BRDA:"):
                parts = line[5:].split(",")
                if len(parts) >= 4:
                    lineno = int(parts[0])
                    taken_str = parts[3]
                    if taken_str == "-":
                        taken = 0
                    else:
                        taken = max(0, int(taken_str))
                    branch_list = current_branches.setdefault(lineno, [])
                    branch = {
                        "count": taken,
                        "fallthrough": False,
                        "throw": False,
                        "source_block_id": int(parts[1]),
                        "destination_block_id": int(parts[2]),
                    }
                    branch_list.append(branch)

            elif line.startswith("FN:"):
                parts = line[3:].split(",", 1)
                if len(parts) == 2:
                    fn_lines[parts[1]] = int(parts[0])

            elif line.startswith("FNDA:"):
                parts = line[5:].split(",", 1)
                if len(parts) == 2:
                    fn_counts[parts[1]] = int(parts[0])

            elif line == "end_of_record":
                if current_file is not None:
                    files.append(_build_file_entry(
                        current_file, current_lines, current_branches,
                        fn_lines, fn_counts))
                current_file = None
                current_lines = {}
                current_branches = {}
                fn_lines = {}
                fn_counts = {}

    if current_file is not None:
        files.append(_build_file_entry(
            current_file, current_lines, current_branches,
            fn_lines, fn_counts))

    return files


def _build_file_entry(
    filename: str,
    lines: Dict[int, Dict[str, Any]],
    branches: Dict[int, List[Dict[str, Any]]],
    fn_lines: Dict[str, int],
    fn_counts: Dict[str, int],
) -> Dict[str, Any]:
    """Build a single gcovr JSON v0.14 file entry."""
    # Attach branches to their respective lines.
    json_lines = []
    for lineno in sorted(lines.keys()):
        line_entry = dict(lines[lineno])
        line_entry["branches"] = branches.get(lineno, [])
        json_lines.append(line_entry)

    # Build functions list.
    json_functions = []
    all_fn_names = set(fn_lines.keys()) | set(fn_counts.keys())
    for name in sorted(all_fn_names):
        func_entry: Dict[str, Any] = {"name": name}
        if name in fn_lines:
            func_entry["lineno"] = fn_lines[name]
        else:
            func_entry["lineno"] = 0
        if name in fn_counts:
            func_entry["execution_count"] = fn_counts[name]
        json_functions.append(func_entry)

    return {
        "file": filename,
        "lines": json_lines,
        "functions": json_functions,
    }


def _load_filter_regexes(path: Path) -> List[re.Pattern]:
    """Load filter regexes from a file (one regex per line, # comments ignored)."""
    if not path.exists():
        print(f"WARNING: Filter regexes file not found: {path}", file=sys.stderr)
        return []
    patterns = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            patterns.append(re.compile(line))
    return patterns


def main() -> None:
    """Main entry point."""
    args = parse_args()

    lcov_path = Path(args.lcov)
    if not lcov_path.exists():
        print(f"ERROR: LCOV file not found: {lcov_path}", file=sys.stderr)
        sys.exit(1)

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    # Parse LCOV into gcovr JSON file entries.
    file_entries = parse_lcov(lcov_path)
    if not file_entries:
        print("WARNING: No coverage data found in LCOV file.", file=sys.stderr)

    # Determine source root for gcovr (--root).
    source_root = args.source_root if args.source_root else os.getcwd()

    # Strip source root from filenames if they're absolute paths.
    root_prefix = source_root.rstrip("/") + "/"
    for entry in file_entries:
        if entry["file"].startswith(root_prefix):
            entry["file"] = entry["file"][len(root_prefix):]
        elif entry["file"].startswith("/"):
            # Absolute path not under source_root — keep as-is, gcovr will
            # resolve relative to --root.
            pass

    # Apply filename filters (exclude files matching any regex).
    if args.filter_regexes:
        filter_patterns = _load_filter_regexes(Path(args.filter_regexes))
        if filter_patterns:
            before = len(file_entries)
            file_entries = [
                entry for entry in file_entries
                if not any(p.search(entry["file"]) for p in filter_patterns)
            ]
            excluded = before - len(file_entries)
            if excluded > 0:
                print(f"Filtered out {excluded} files ({len(file_entries)} remaining)")

    # Write gcovr JSON v0.14 tracefile.
    gcovr_json = {
        "gcovr/format_version": "0.14",
        "files": file_entries,
    }

    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".json", prefix="lcov_to_gcovr_", delete=False
    ) as tmp:
        json.dump(gcovr_json, tmp)
        tracefile_path = tmp.name

    try:
        # Invoke gcovr to generate HTML from the JSON tracefile.
        output_file = str(output_dir / "index.html")
        gcovr_args = [
            "gcovr",
            "--add-tracefile", tracefile_path,
            "--html-details", output_file,
            "--root", source_root,
        ]

        print(f"Running gcovr with {len(file_entries)} files...")
        sys.argv = gcovr_args
        from gcovr.__main__ import main as gcovr_main
        try:
            gcovr_main()
        except SystemExit as e:
            if e.code != 0:
                print(f"ERROR: gcovr exited with code {e.code}", file=sys.stderr)
                sys.exit(e.code)

        print(f"HTML coverage report written to: {output_dir}")
    finally:
        os.unlink(tracefile_path)


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Generate HTML coverage report from LCOV data using gcovr")
    parser.add_argument("--lcov", required=True,
                        help="Path to LCOV .dat file")
    parser.add_argument("--output-dir", required=True,
                        help="Directory for HTML output")
    parser.add_argument("--source-root", default="",
                        help="Source root for resolving relative paths (default: cwd)")
    parser.add_argument("--filter-regexes", default="",
                        help="Path to file with regexes (one per line) to exclude from report")
    return parser.parse_args()


if __name__ == "__main__":
    main()
