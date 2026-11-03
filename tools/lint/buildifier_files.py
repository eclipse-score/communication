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

"""Resolves Bazel-style package patterns to the Starlark files buildifier checks.

Supported patterns: `//pkg`, `//pkg:target` and `//pkg/...`, each optionally
prefixed with `-` to exclude the files it resolves to.
"""

from pathlib import Path

_SUFFIXES = (".bzl", ".bazel", ".sky", ".BUILD")


def _is_starlark_file(path: Path) -> bool:
    return path.name == "BUILD" or path.name.endswith(_SUFFIXES)


def _is_skipped_dir(path: Path) -> bool:
    # Hidden directories (.git, IDE state) hold no sources, and symlinked
    # directories (the bazel-* output links) lead out of the source tree.
    return path.name.startswith(".") or path.is_symlink()


def _resolve(workspace: Path, pattern: str) -> set[Path]:
    if not pattern.startswith("//"):
        raise ValueError(f"Unsupported pattern (expected [-]//pkg[:target] or [-]//pkg/...): {pattern}")
    package = pattern[2:].split(":", 1)[0]
    recursive = package == "..." or package.endswith("/...")
    if recursive:
        package = package[: -len("...")].rstrip("/")
    root = workspace / package
    if not root.is_dir():
        raise ValueError(f"No such package directory: {package or '.'}")

    if not recursive:
        return {p for p in root.iterdir() if p.is_file() and _is_starlark_file(p)}

    files = set()
    stack = [root]
    while stack:
        for entry in stack.pop().iterdir():
            if entry.is_dir():
                if not _is_skipped_dir(entry):
                    stack.append(entry)
            elif _is_starlark_file(entry):
                files.add(entry)
    return files


def resolve_files(workspace: Path, patterns: list[str]) -> list[Path]:
    """Returns the sorted Starlark files selected by `patterns`.

    Raises ValueError for unsupported patterns, unknown directories or an
    empty selection.
    """
    included: set[Path] = set()
    excluded: set[Path] = set()
    for pattern in patterns:
        if pattern.startswith("-"):
            excluded |= _resolve(workspace, pattern[1:])
        else:
            included |= _resolve(workspace, pattern)
    selected = sorted(included - excluded)
    if not selected:
        raise ValueError(f"No Starlark files found for: {' '.join(patterns)}")
    return selected
