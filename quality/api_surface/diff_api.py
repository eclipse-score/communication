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

"""Compare two API surface JSON files."""

from __future__ import annotations

import argparse
import difflib
import json
import sys
from pathlib import Path


def _should_check_docs(argv: list[str]) -> bool:
    return "--check-docs" in argv


def _load_json(path: str) -> dict:
    with Path(path).open(encoding="utf-8") as f:
        return json.load(f)


def _normalized_symbols(data: dict) -> str:
    symbols = [
        {
            "kind": symbol["kind"],
            "name": symbol["name"],
            "qualified_name": symbol["qualified_name"],
            "signature": symbol["signature"],
        }
        for symbol in data.get("symbols", [])
    ]
    return json.dumps(symbols, indent=2, sort_keys=True)


def _format_undocumented_symbols(data: dict) -> str:
    undocumented = data.get("undocumented_symbols", [])
    if not undocumented:
        return ""

    lines = [
        "ERROR: The following public symbols are missing \\api documentation:",
        "",
    ]
    for symbol in undocumented:
        lines.append(f"  {symbol['qualified_name']} ({symbol['kind']})")
        lines.append(f"    at {symbol['file']}:{symbol['line']}")
    lines.extend(
        [
            "",
            f"Total: {len(undocumented)} undocumented public symbol(s)",
            "",
            "Fix: Add \\api and \\brief doxygen tags to these declarations.",
        ]
    )
    return "\n".join(lines)


def compare(lock_file: str, current_file: str, check_docs: bool = False) -> int:
    lock_data = _load_json(lock_file)
    current_data = _load_json(current_file)

    if check_docs:
        undocumented = _format_undocumented_symbols(current_data)
        if undocumented:
            print(undocumented)
            return 1

    lock_symbols = _normalized_symbols(lock_data)
    current_symbols = _normalized_symbols(current_data)

    if lock_symbols == current_symbols:
        print("API surface check PASSED: No changes detected.")
        return 0

    print("ERROR: API surface has changed!\n")
    print("Differences between lock file and current API:\n")
    for line in difflib.unified_diff(
        lock_symbols.splitlines(),
        current_symbols.splitlines(),
        fromfile=lock_file,
        tofile=current_file,
        lineterm="",
    ):
        print(line)
    print("\nIf this change is intentional, update the lock file:\n")
    print("  bazel run //score/mw/com:api_surface_update")
    print("\n  (or the corresponding _update target for your library)\n")
    return 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("lock_file")
    parser.add_argument("current_file")
    parser.add_argument("--check-docs", action="store_true")
    args = parser.parse_args(argv)
    return compare(args.lock_file, args.current_file, args.check_docs)


if __name__ == "__main__":
    raise SystemExit(main())
