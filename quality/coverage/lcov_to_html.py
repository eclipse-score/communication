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
from typing import Any, Dict, List, Optional, Sequence, Set, Tuple

PROC_SELF_CWD_PREFIX = "/proc/self/cwd/"


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


def _load_allowlist(path: Path) -> Set[str]:
    """Load workspace-relative file paths from an allowlist file."""
    if not path.exists():
        print(f"WARNING: Allowlist file not found: {path}", file=sys.stderr)
        return set()

    entries = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            entries.add(line)
    return entries


def _normalize_entry_path(filename: str, source_root: str) -> str:
    """Normalize LCOV paths to workspace-relative paths for comparison/output."""
    normalized_source_root = source_root.rstrip("/")
    if normalized_source_root and filename.startswith(normalized_source_root + "/"):
        return filename[len(normalized_source_root) + 1:]
    if filename.startswith(PROC_SELF_CWD_PREFIX):
        return filename[len(PROC_SELF_CWD_PREFIX):]
    if filename.startswith("./"):
        return filename[2:]
    return filename


def _normalize_file_entries(
    file_entries: Sequence[Dict[str, Any]],
    source_root: str,
) -> List[Dict[str, Any]]:
    """Normalize all file paths in LCOV entries."""
    normalized: List[Dict[str, Any]] = []
    for entry in file_entries:
        updated = dict(entry)
        updated["file"] = _normalize_entry_path(entry["file"], source_root)
        normalized.append(updated)
    return normalized


def _filter_file_entries_by_allowlist(
    file_entries: Sequence[Dict[str, Any]],
    allowlist: Set[str],
    source_root: str,
) -> List[Dict[str, Any]]:
    """Keep LCOV entries in allowlist; return all entries unchanged when allowlist is empty."""
    if not allowlist:
        return list(file_entries)

    filtered: List[Dict[str, Any]] = []
    for entry in file_entries:
        normalized = _normalize_entry_path(entry["file"], source_root)
        if normalized in allowlist:
            updated = dict(entry)
            updated["file"] = normalized
            filtered.append(updated)
    return filtered


def _merge_file_entries(
    primary_entries: Sequence[Dict[str, Any]],
    baseline_entries: Sequence[Dict[str, Any]],
) -> List[Dict[str, Any]]:
    """Merge primary LCOV entries with baseline entries.

    Primary entries keep precedence for execution counts; baseline contributes
    missing files/lines with zero-hit data.
    """
    merged_by_file: Dict[str, Dict[str, Any]] = {}
    order: List[str] = []

    for entry in list(primary_entries) + list(baseline_entries):
        filename = entry["file"]
        if filename not in merged_by_file:
            merged_by_file[filename] = {
                "file": filename,
                "lines": [],
                "functions": [],
            }
            order.append(filename)
        _merge_single_file_entry(merged_by_file[filename], entry)

    return [merged_by_file[filename] for filename in order]


def _merge_single_file_entry(target: Dict[str, Any], incoming: Dict[str, Any]) -> None:
    """Merge one file entry into another in-place."""
    target["lines"] = _merge_line_entries(
        target.get("lines", []),
        incoming.get("lines", []),
    )
    target["functions"] = _merge_function_entries(
        target.get("functions", []),
        incoming.get("functions", []),
    )


def _clone_line_entry(line_entry: Dict[str, Any]) -> Dict[str, Any]:
    """Copy line entries with branch data to avoid mutating shared dictionaries."""
    cloned = dict(line_entry)
    cloned["branches"] = [dict(branch) for branch in line_entry.get("branches", [])]
    return cloned


def _merge_line_entries(
    existing_lines: Sequence[Dict[str, Any]],
    incoming_lines: Sequence[Dict[str, Any]],
) -> List[Dict[str, Any]]:
    """Merge line coverage and branch data for a single source file.

    We keep the maximum execution count per line/branch identity instead of summing:
    the same line can appear in multiple LCOV inputs for identical instrumentation,
    and summing those overlapping records inflates execution counts.
    """
    merged_lines = {
        line["line_number"]: _clone_line_entry(line) for line in existing_lines
    }
    for incoming_line in incoming_lines:
        line_number = incoming_line["line_number"]
        incoming_copy = _clone_line_entry(incoming_line)

        if line_number not in merged_lines:
            merged_lines[line_number] = incoming_copy
            continue

        existing = merged_lines[line_number]
        existing["count"] = max(existing.get("count", 0), incoming_copy.get("count", 0))
        existing["branches"] = _merge_branch_entries(
            existing.get("branches", []),
            incoming_copy.get("branches", []),
        )

    return sorted(merged_lines.values(), key=lambda line: line["line_number"])


def _merge_function_entries(
    existing_functions: Sequence[Dict[str, Any]],
    incoming_functions: Sequence[Dict[str, Any]],
) -> List[Dict[str, Any]]:
    """Merge function coverage for a single source file."""
    merged_functions = {func["name"]: dict(func) for func in existing_functions}
    for incoming_func in incoming_functions:
        name = incoming_func["name"]
        incoming_lineno = incoming_func.get("lineno", 0)
        incoming_execution_count = incoming_func.get("execution_count", 0)

        if name not in merged_functions:
            merged_functions[name] = dict(incoming_func)
            continue

        existing_func = merged_functions[name]
        if existing_func.get("lineno", 0) == 0 and incoming_lineno != 0:
            existing_func["lineno"] = incoming_lineno
        existing_func["execution_count"] = max(
            existing_func.get("execution_count", 0),
            incoming_execution_count,
        )

    return sorted(merged_functions.values(), key=lambda func: func["name"])


def _merge_branch_entries(
    existing_branches: Sequence[Dict[str, Any]],
    incoming_branches: Sequence[Dict[str, Any]],
) -> List[Dict[str, Any]]:
    """Merge branch lists and keep max execution count per branch identity."""
    merged: Dict[Tuple[Any, Any, Any, Any], Dict[str, Any]] = {}
    for branch in list(existing_branches) + list(incoming_branches):
        key = (
            branch.get("source_block_id"),
            branch.get("destination_block_id"),
            branch.get("fallthrough", False),
            branch.get("throw", False),
        )
        branch_copy = dict(branch)
        if key in merged:
            merged[key]["count"] = max(merged[key].get("count", 0), branch_copy.get("count", 0))
        else:
            merged[key] = branch_copy
    return list(merged.values())


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
    file_entries = _normalize_file_entries(file_entries, source_root)

    if args.baseline_lcov:
        baseline_lcov_path = Path(args.baseline_lcov)
        if not baseline_lcov_path.exists():
            print(f"ERROR: Baseline LCOV file not found: {baseline_lcov_path}", file=sys.stderr)
            sys.exit(1)
        baseline_entries = parse_lcov(baseline_lcov_path)
        if baseline_entries:
            baseline_entries = _normalize_file_entries(baseline_entries, source_root)
            file_entries = _merge_file_entries(file_entries, baseline_entries)
            print(
                f"Merged baseline LCOV from {baseline_lcov_path} "
                f"({len(baseline_entries)} file records)"
            )

    allowlist: Set[str] = set()
    if args.allowlist:
        allowlist = _load_allowlist(Path(args.allowlist))
        if not allowlist:
            print(f"ERROR: Allowlist is empty: {args.allowlist}", file=sys.stderr)
            sys.exit(1)
        before = len(file_entries)
        file_entries = _filter_file_entries_by_allowlist(file_entries, allowlist, source_root)
        removed = before - len(file_entries)
        if removed > 0:
            print(f"Filtered out {removed} files outside the allowlist ({len(file_entries)} remaining)")

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
    parser.add_argument("--allowlist", default="",
                        help="Path to workspace-relative file allowlist (one path per line)")
    parser.add_argument("--baseline-lcov", default="",
                        help="Path to LCOV baseline tracefile to merge before filtering")
    return parser.parse_args()


if __name__ == "__main__":
    main()
