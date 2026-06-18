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
"""Coverage justification processor.

Parses the YAML justification database and source files for COV_JUSTIFIED markers.
Resolves all justified lines and produces a manifest mapping file:line → justification.

Usage:
    python justify.py --yaml <justifications.yaml> --source-root <path> --output <manifest.json>

Supports two ways to specify justified lines:
1. YAML locations: directly specify file + line ranges in the YAML
2. In-code markers: COV_JUSTIFIED <id>, COV_JUSTIFIED_START <id> / COV_JUSTIFIED_STOP
"""

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, List, Set, Tuple

import yaml


# Marker patterns
COV_JUSTIFIED_LINE_RE = re.compile(r"COV_JUSTIFIED\s+([\w-]+)")
COV_JUSTIFIED_START_RE = re.compile(r"COV_JUSTIFIED_START\s+([\w-]+)")
COV_JUSTIFIED_STOP_RE = re.compile(r"COV_JUSTIFIED_STOP")

VALID_CATEGORIES = {
    "defensive_programming",
    "tool_false_positive",
    "platform_specific",
    "other",
}


def main() -> None:
    """Main entry point."""
    args = parse_args()

    justifications_data = load_yaml(args.yaml)
    validate_yaml(justifications_data)

    # Build lookup: id -> justification entry
    justifications_by_id: Dict[str, Dict[str, Any]] = {}
    for entry in justifications_data.get("justifications", []):
        justifications_by_id[entry["id"]] = entry

    # Resolve all justified lines
    resolved: Dict[str, Dict[int, Dict[str, str]]] = {}
    warnings: List[str] = []
    errors: List[str] = []

    # 1. Process YAML direct locations
    for entry in justifications_data.get("justifications", []):
        for location in entry.get("locations", []):
            file_path = location["file"]
            full_path = Path(args.source_root) / file_path

            if not full_path.exists():
                errors.append(
                    f"File not found for justification '{entry['id']}': {file_path}"
                )
                continue

            lines = resolve_location_lines(location)
            if file_path not in resolved:
                resolved[file_path] = {}
            for line in lines:
                resolved[file_path][line] = {
                    "id": entry["id"],
                    "category": entry["category"],
                    "reason": entry["reason"].strip(),
                }

    # 2. Scan source files for in-code COV_JUSTIFIED markers
    source_files = collect_source_files(args.source_root, args.file_filter)
    for source_file in source_files:
        rel_path = str(source_file.relative_to(args.source_root))
        scan_warnings, scan_lines = scan_file_for_markers(
            source_file, rel_path, justifications_by_id
        )
        warnings.extend(scan_warnings)

        if scan_lines:
            if rel_path not in resolved:
                resolved[rel_path] = {}
            for line_num, justification_info in scan_lines.items():
                resolved[rel_path][line_num] = justification_info

    # Output manifest
    manifest = {
        "version": 1,
        "source_root": str(args.source_root),
        "justified_files": {
            filepath: {str(k): v for k, v in lines.items()}
            for filepath, lines in sorted(resolved.items())
        },
        "warnings": warnings,
        "errors": errors,
    }

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    # Print diagnostics
    total_justified_lines = sum(len(lines) for lines in resolved.values())
    print(
        f"INFO: Resolved {total_justified_lines} justified lines across "
        f"{len(resolved)} files.",
        file=sys.stderr,
    )
    if warnings:
        for w in warnings:
            print(f"WARNING: {w}", file=sys.stderr)
    if errors:
        for e in errors:
            print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)


def resolve_location_lines(location: Dict[str, Any]) -> List[int]:
    """Resolve line numbers from a YAML location entry."""
    if "lines" in location:
        return location["lines"]
    elif "line_start" in location and "line_end" in location:
        return list(range(location["line_start"], location["line_end"] + 1))
    elif "line" in location:
        return [location["line"]]
    return []


def scan_file_for_markers(
    file_path: Path,
    rel_path: str,
    justifications_by_id: Dict[str, Dict[str, Any]],
) -> Tuple[List[str], Dict[int, Dict[str, str]]]:
    """Scan a source file for COV_JUSTIFIED markers."""
    warnings = []
    justified_lines: Dict[int, Dict[str, str]] = {}

    try:
        with open(file_path, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except (IOError, OSError):
        return warnings, justified_lines

    region_stack: List[Tuple[int, str]] = []  # (start_line, justification_id)

    for line_num, line in enumerate(lines, start=1):
        # Check for COV_JUSTIFIED_START
        start_match = COV_JUSTIFIED_START_RE.search(line)
        if start_match:
            jid = start_match.group(1)
            if jid not in justifications_by_id:
                warnings.append(
                    f"{rel_path}:{line_num}: COV_JUSTIFIED_START references "
                    f"unknown ID '{jid}'"
                )
            else:
                region_stack.append((line_num, jid))
            continue

        # Check for COV_JUSTIFIED_STOP
        stop_match = COV_JUSTIFIED_STOP_RE.search(line)
        if stop_match:
            if not region_stack:
                warnings.append(
                    f"{rel_path}:{line_num}: COV_JUSTIFIED_STOP without matching START"
                )
            else:
                start_line, jid = region_stack.pop()
                if jid in justifications_by_id:
                    entry = justifications_by_id[jid]
                    for ln in range(start_line + 1, line_num):
                        justified_lines[ln] = {
                            "id": jid,
                            "category": entry["category"],
                            "reason": entry["reason"].strip(),
                        }
            continue

        # Check for single-line COV_JUSTIFIED (but not START/STOP)
        if "COV_JUSTIFIED_START" not in line and "COV_JUSTIFIED_STOP" not in line:
            line_match = COV_JUSTIFIED_LINE_RE.search(line)
            if line_match:
                jid = line_match.group(1)
                if jid not in justifications_by_id:
                    warnings.append(
                        f"{rel_path}:{line_num}: COV_JUSTIFIED references "
                        f"unknown ID '{jid}'"
                    )
                else:
                    entry = justifications_by_id[jid]
                    justified_lines[line_num] = {
                        "id": jid,
                        "category": entry["category"],
                        "reason": entry["reason"].strip(),
                    }

    # Check for unclosed regions
    for start_line, jid in region_stack:
        warnings.append(
            f"{rel_path}:{start_line}: COV_JUSTIFIED_START '{jid}' without matching STOP"
        )

    return warnings, justified_lines


def collect_source_files(source_root: Path, file_filter: str) -> List[Path]:
    """Collect source files to scan for markers."""
    extensions = file_filter.split(",") if file_filter else ["cpp", "h", "hpp", "cc"]
    files = []
    for ext in extensions:
        files.extend(source_root.rglob(f"*.{ext.strip()}"))
    return sorted(files)


def load_yaml(yaml_path: Path) -> Dict[str, Any]:
    """Load YAML justification database."""
    if not yaml_path.exists():
        print(f"ERROR: Justification YAML not found: {yaml_path}", file=sys.stderr)
        sys.exit(1)

    with open(yaml_path, "r", encoding="utf-8") as f:
        content = f.read()

    return yaml.safe_load(content)


def validate_yaml(data: Dict[str, Any]) -> None:
    """Validate the justification YAML structure and types."""
    try:
        errors = []

        if not isinstance(data, dict):
            print("ERROR: YAML validation: root must be a mapping", file=sys.stderr)
            sys.exit(1)

        if "version" not in data:
            errors.append("Missing 'version' field")
        elif not isinstance(data["version"], int):
            errors.append(f"'version' must be an integer, got {type(data['version']).__name__}")

        if "justifications" not in data:
            errors.append("Missing 'justifications' field")
            for e in errors:
                print(f"ERROR: {e}", file=sys.stderr)
            sys.exit(1)

        if not isinstance(data["justifications"], list):
            errors.append(
                f"'justifications' must be a list, got {type(data['justifications']).__name__}"
            )
            for e in errors:
                print(f"ERROR: YAML validation: {e}", file=sys.stderr)
            sys.exit(1)

        seen_ids: Set[str] = set()
        for i, entry in enumerate(data["justifications"]):
            prefix = f"justifications[{i}]"

            if not isinstance(entry, dict):
                errors.append(f"{prefix}: must be a mapping, got {type(entry).__name__}")
                continue

            if "id" not in entry:
                errors.append(f"{prefix}: missing 'id'")
                continue

            jid = entry["id"]
            if not isinstance(jid, str):
                errors.append(f"{prefix}: 'id' must be a string, got {type(jid).__name__}")
                continue

            if jid in seen_ids:
                errors.append(f"{prefix}: duplicate ID '{jid}'")
            seen_ids.add(jid)

            if not re.match(r"^[a-z0-9]+(-[a-z0-9]+)*$", jid):
                errors.append(f"{prefix}: ID '{jid}' must be kebab-case")

            if "category" not in entry:
                errors.append(f"{prefix}: missing 'category'")
            elif not isinstance(entry["category"], str):
                errors.append(
                    f"{prefix}: 'category' must be a string, "
                    f"got {type(entry['category']).__name__}"
                )
            elif entry["category"] not in VALID_CATEGORIES:
                errors.append(
                    f"{prefix}: invalid category '{entry['category']}'. "
                    f"Must be one of: {sorted(VALID_CATEGORIES)}"
                )

            if "reason" not in entry:
                errors.append(f"{prefix}: missing 'reason'")
            elif not isinstance(entry["reason"], str):
                errors.append(
                    f"{prefix}: 'reason' must be a string, "
                    f"got {type(entry['reason']).__name__}"
                )
            elif not entry["reason"].strip():
                errors.append(f"{prefix}: 'reason' must not be empty")

            if "locations" in entry:
                if not isinstance(entry["locations"], list):
                    errors.append(
                        f"{prefix}: 'locations' must be a list, "
                        f"got {type(entry['locations']).__name__}"
                    )
                else:
                    for j, loc in enumerate(entry["locations"]):
                        loc_prefix = f"{prefix}.locations[{j}]"
                        if not isinstance(loc, dict):
                            errors.append(
                                f"{loc_prefix}: must be a mapping, "
                                f"got {type(loc).__name__}"
                            )
                            continue
                        if "file" not in loc:
                            errors.append(f"{loc_prefix}: missing 'file'")
                        elif not isinstance(loc["file"], str):
                            errors.append(
                                f"{loc_prefix}: 'file' must be a string, "
                                f"got {type(loc['file']).__name__}"
                            )
                        for int_field in ("line", "line_start", "line_end"):
                            if int_field in loc and not isinstance(loc[int_field], int):
                                errors.append(
                                    f"{loc_prefix}: '{int_field}' must be an integer, "
                                    f"got {type(loc[int_field]).__name__}"
                                )
                        if "lines" in loc:
                            if not isinstance(loc["lines"], list):
                                errors.append(
                                    f"{loc_prefix}: 'lines' must be a list, "
                                    f"got {type(loc['lines']).__name__}"
                                )
                            elif not all(isinstance(ln, int) for ln in loc["lines"]):
                                errors.append(
                                    f"{loc_prefix}: 'lines' must contain only integers"
                                )

        if errors:
            for e in errors:
                print(f"ERROR: YAML validation: {e}", file=sys.stderr)
            sys.exit(1)
    except Exception as error:
        print(f"ERROR: YAML validation: {error}", file=sys.stderr)
        sys.exit(1)


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Coverage justification processor"
    )
    parser.add_argument(
        "--yaml",
        type=Path,
        required=True,
        help="Path to coverage_justifications.yaml",
    )
    parser.add_argument(
        "--source-root",
        type=Path,
        required=True,
        help="Root directory of source files",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Output path for resolved justification manifest (JSON)",
    )
    parser.add_argument(
        "--file-filter",
        type=str,
        default="cpp,h,hpp,cc",
        help="Comma-separated file extensions to scan (default: cpp,h,hpp,cc)",
    )
    return parser.parse_args()


if __name__ == "__main__":
    main()
