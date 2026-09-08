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

"""Rewrite the root MODULE.bazel to pin dependency versions for one combination."""

import argparse
import json
import re
import sys
from pathlib import Path

from quality.dependency_compatibility_checker import config_schema

_OVERRIDE_FUNCS = (
    "single_version_override",
    "git_override",
    "archive_override",
    "local_path_override",
)


def _is_comment(line: str) -> bool:
    return line.lstrip().startswith("#")


def _paren_delta(line: str) -> int:
    """Net change in parenthesis depth for a single line.

    Parentheses inside string literals or trailing ``#`` comments are ignored so
    that a stray bracket in a comment or string value cannot fool the block
    scanner. Handles single- and double-quoted strings with backslash escapes,
    which covers everything buildifier emits in override blocks.
    """
    depth = 0
    quote = None  # active string delimiter, one of ' or "
    i = 0
    n = len(line)
    while i < n:
        ch = line[i]
        if quote is not None:
            if ch == "\\":
                i += 2
                continue
            if ch == quote:
                quote = None
        elif ch == "#":
            break
        elif ch in ("'", '"'):
            quote = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        i += 1
    return depth


def _block_end(lines, start: int) -> int:
    """Return the index of the last line of the parenthesised call starting at `start`."""
    depth = _paren_delta(lines[start])
    end = start
    while depth > 0:
        end += 1
        if end >= len(lines):
            raise ValueError(f"Unterminated call starting at line {start + 1}")
        depth += _paren_delta(lines[end])
    return end


def _remove_override_blocks(lines, module_name: str):
    """Remove every override block whose module_name matches (non-comment lines only)."""
    name_re = re.compile(r"""\bmodule_name\s*=\s*['"]""" + re.escape(module_name) + r"""['"]""")
    starts_re = re.compile(r"^\s*(?:%s)\s*\(" % "|".join(_OVERRIDE_FUNCS))
    i = 0
    out = []
    while i < len(lines):
        line = lines[i]
        if not _is_comment(line) and starts_re.match(line):
            end = _block_end(lines, i)
            body = [ln for ln in lines[i : end + 1] if not _is_comment(ln)]
            if any(name_re.search(ln) for ln in body):
                i = end + 1
                # drop a single trailing blank line left by the removal
                if i < len(lines) and lines[i].strip() == "":
                    i += 1
                continue
        out.append(line)
        i += 1
    return out


def _format_override(module_name: str, entry) -> list:
    block = [
        "single_version_override(",
        f'    module_name = "{module_name}",',
        f'    version = "{entry.version}",',
    ]
    if entry.patches:
        if entry.patch_strip is not None:
            block.append(f"    patch_strip = {entry.patch_strip},")
        block.append("    patches = [")
        for patch in entry.patches:
            block.append(f'        "{patch}",')
        block.append("    ],")
    block.append(")")
    return block


def rewrite(module_file, config_path, combination: dict):
    cfg = config_schema.load_config(config_path)
    lines = Path(module_file).read_text(encoding="utf-8").splitlines()

    blocks = []
    for dep in cfg.dependencies:
        if dep.is_bazel or dep.key not in combination:
            continue
        version = combination[dep.key]
        entry = next((v for v in dep.versions if v.version == version), None)
        if entry is None:
            raise ValueError(f"{dep.key}: version {version} not found in config")
        lines = _remove_override_blocks(lines, dep.module_name)
        blocks.append(_format_override(dep.module_name, entry))

    # Emit buildifier-canonical output: each override is a multi-line block and
    # consecutive blocks are separated by a blank line, so the repository's
    # buildifier format_test still passes after the rewrite.
    appended = []
    for block in blocks:
        if appended:
            appended.append("")
        appended.extend(block)

    while lines and lines[-1].strip() == "":
        lines.pop()
    if lines:
        lines.append("")
    lines.extend(appended)

    Path(module_file).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Rewrote {module_file}: appended {len(blocks)} override block(s)")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--module-file", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--combination-json", required=True)
    args = parser.parse_args(argv)
    rewrite(args.module_file, args.config, json.loads(args.combination_json))
    return 0


if __name__ == "__main__":
    sys.exit(main())
