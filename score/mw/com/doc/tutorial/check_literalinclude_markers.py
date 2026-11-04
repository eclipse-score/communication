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

"""Check the literalinclude markers used in the tutorial chapters.

For every ".. literalinclude::" in a tutorial .rst file that uses
":start-after:" / ":end-before:" this script checks that
  - the included file exists,
  - both options are given,
  - each marker text appears exactly once in the included file,
  - the start marker comes before the end marker,
  - each marker is on its own line, like "// [name-start]" or "# [name-end]".
":lines:" is not allowed, except for .json files.
"""

import os
import re
import sys

DIRECTIVE = re.compile(r"^(\s*)\.\. literalinclude::\s*(\S+)\s*$")
# A marker must be a whole line, e.g. "// [my-name-start]" or "# [my-name-end]".
MARKER_LINE = re.compile(r"^\s*(//|#) \[[a-z0-9]+(-[a-z0-9]+)*-(start|end)\]$")
LINES_ALLOWED = (".json",)
OPTION = re.compile(r"^\s+:([a-z-]+):\s*(.*?)\s*$")


def find_directives(rst_path):
    """Return a list of (line_number, included_path, options) for one .rst file."""
    with open(rst_path, encoding="utf-8") as rst_file:
        lines = rst_file.read().splitlines()
    directives = []
    index = 0
    while index < len(lines):
        match = DIRECTIVE.match(lines[index])
        if match is None:
            index += 1
            continue
        start_line = index + 1
        options = {}
        index += 1
        # The options directly follow the directive line and are indented.
        while index < len(lines):
            option = OPTION.match(lines[index])
            if option is None:
                break
            options[option.group(1)] = option.group(2)
            index += 1
        directives.append((start_line, match.group(2), options))
    return directives


def check_directive(rst_path, line_number, included, options):
    """Return a list of error messages for one literalinclude directive."""
    where = f"{rst_path}:{line_number}"
    start_marker = options.get("start-after")
    end_marker = options.get("end-before")
    if "lines" in options and not included.endswith(LINES_ALLOWED):
        return [f"{where}: ':lines:' is not allowed for '{included}', use markers instead"]
    if start_marker is None and end_marker is None:
        return []  # includes the whole file or uses :lines: on a .json file
    if start_marker is None or end_marker is None:
        return [f"{where}: ':start-after:' and ':end-before:' must be used together"]
    target = os.path.join(os.path.dirname(rst_path), included)
    if not os.path.isfile(target):
        return [f"{where}: included file '{included}' does not exist"]
    with open(target, encoding="utf-8") as target_file:
        target_lines = target_file.read().splitlines()
    errors = []
    positions = {}
    for name, marker in (("start-after", start_marker), ("end-before", end_marker)):
        found = [i for i, text in enumerate(target_lines) if marker in text]
        if len(found) != 1:
            errors.append(
                f"{where}: {name} marker '{marker}' found {len(found)} times in '{included}', expected exactly once"
            )
        else:
            positions[name] = found[0]
            if MARKER_LINE.match(target_lines[found[0]]) is None:
                errors.append(
                    f"{where}: {name} marker '{marker}' in '{included}' must be on its own line, like '// {marker}'"
                )
    if len(positions) == 2 and positions["start-after"] >= positions["end-before"]:
        errors.append(f"{where}: start marker '{start_marker}' must come before end marker '{end_marker}'")
    return errors


def main(argv):
    root = argv[1] if len(argv) > 1 else os.path.dirname(os.path.abspath(__file__))
    errors = []
    checked = 0
    for directory, _, files in sorted(os.walk(root)):
        for name in sorted(files):
            if not name.endswith(".rst"):
                continue
            rst_path = os.path.join(directory, name)
            for line_number, included, options in find_directives(rst_path):
                checked += 1
                errors.extend(check_directive(rst_path, line_number, included, options))
    for error in errors:
        print(error, file=sys.stderr)
    print(f"Checked {checked} literalinclude directives, {len(errors)} errors.")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
