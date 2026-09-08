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

"""Clang-tidy and ruff aspects, plus developer check/fix targets."""

load("@aspect_rules_lint//lint:clang_tidy.bzl", "lint_clang_tidy_aspect")
load("@aspect_rules_lint//lint:lint_test.bzl", "lint_test")
load("@aspect_rules_lint//lint:ruff.bzl", "lint_ruff_aspect")
load("@bazel_skylib//rules:write_file.bzl", "write_file")

visibility(["//..."])

# Define the clang-tidy linter aspect using LLVM toolchain
# NOTE: lint_target_headers is deliberately left False (the aspect_rules_lint
# default). When enabled it computes its own `-header-filter` regex from the
# target's own header directories, which unconditionally overrides (rather
# than merges with) //:.clang-tidy's HeaderFilterRegex, and falls back to
# matching *every* header (".*") whenever a target has more than one header
# directory (e.g. because of _virtual_includes) -- letting findings from
# external Bazel repositories (bazel-out/.../external/<repo>/...), which we
# cannot fix or influence, leak into the results.
#
# header_filter is set explicitly (rather than relying on clang-tidy
# discovering //:.clang-tidy's HeaderFilterRegex on its own, which isn't
# reliable from inside the sandbox) to the same pattern as
# //:.clang-tidy's HeaderFilterRegex, so our own headers are still linted
# while third_party/ and external/ (Bazel external repositories) are
# excluded.
clang_tidy = lint_clang_tidy_aspect(
    binary = Label("@llvm_toolchain//:clang-tidy"),
    configs = [
        Label("//:.clang-tidy"),
    ],
    header_filter = "^(?!(third_party|external)/).*$",
    angle_includes_are_system = True,
    verbose = False,
)

# Create a test rule for clang-tidy (for individual targets)
clang_tidy_test = lint_test(aspect = clang_tidy)

# Define the ruff linter aspect for Python targets. The repo's //:.ruff.toml
# is passed so the lint aspect and `ruff format` read the same config file,
# but that file currently has no [lint]/[lint.*] tables, so ruff still falls
# back to its own built-in default rule selection
# (https://docs.astral.sh/ruff/rules/#default-rules). Wiring the file in now
# means a future project-specific lint override only requires adding a
# [lint] table to //:.ruff.toml, not touching this aspect definition.
ruff = lint_ruff_aspect(
    binary = Label("@aspect_rules_lint//lint:ruff_bin"),
    configs = [Label("//:.ruff.toml")],
)

APPLY_PATCHES = [
    'echo ""',
    'echo "=== Applying patches ==="',
    "mapfile -d '' PATCH_FILES < <(find -L bazel-bin -name '*.AspectRulesLintClangTidy.patch' -size +0c -print0 2>/dev/null | sort -z)",
    "if [[ ${#PATCH_FILES[@]} -eq 0 ]]; then",
    '    echo "No auto-fixable violations found."',
    '    [[ ${BAZEL_EXIT} -ne 0 ]] && echo "clang-tidy reported diagnostics that were not auto-fixable."',
    "    exit ${BAZEL_EXIT}",
    "fi",
    "APPLIED=0; SKIPPED=0",
    'for patch in "${PATCH_FILES[@]}"; do',
    '    if git apply --ignore-whitespace "${patch}" 2>/dev/null; then echo "  applied: ${patch}"; APPLIED=$((APPLIED + 1))',
    '    else echo "  skipped: ${patch}"; SKIPPED=$((SKIPPED + 1)); fi',
    "done",
    'echo ""; echo "  Patches applied : ${APPLIED}"',
    '[[ ${SKIPPED} -gt 0 ]] && echo "  Patches skipped : ${SKIPPED}"',
    'echo "Review with: git diff"; echo "Stage with : git add -p"',
    '[[ ${BAZEL_EXIT} -ne 0 ]] && echo "clang-tidy reported diagnostics that were not auto-fixable."',
    "exit ${BAZEL_EXIT}",
]

def make_script(name, content):
    write_file(
        name = name + "_script",
        out = name.replace("-", "_") + ".sh",
        is_executable = True,
        content = ["#!/usr/bin/env bash", "set -euo pipefail", 'TARGETS="${*:-//...}"', 'cd "${BUILD_WORKSPACE_DIRECTORY}"'] + content,
    )

def use_clang_tidy_targets(fix_name = "clang-tidy.fix", check_name = "clang-tidy.check"):
    """Declare clang-tidy check and fix script targets."""
    make_script(fix_name, [
        'echo "=== clang-tidy autofix: ${TARGETS} ==="',
        "find -L bazel-bin -name '*.AspectRulesLintClangTidy.patch' -delete 2>/dev/null || true",
        "BAZEL_EXIT=0",
        "bazel test --config=clang-tidy-fix ${TARGETS} || BAZEL_EXIT=$?",
    ] + APPLY_PATCHES)

    make_script(check_name, [
        'echo "=== clang-tidy check: ${TARGETS} ==="',
        "bazel test --config=clang-tidy ${TARGETS}",
    ])

def use_ruff_targets(fix_name = "ruff.fix", check_name = "ruff.check"):
    """Declare ruff check and fix script targets.

    Unlike clang-tidy/pylint, `aspect_rules_lint`'s ruff integration can apply
    fixes directly (via `ruff check --fix`) instead of emitting a patch file,
    so `ruff.fix` runs bazel with `--config=ruff-fix` and is done.
    """
    make_script(fix_name, [
        'echo "=== ruff autofix: ${TARGETS} ==="',
        "bazel test --config=ruff-fix ${TARGETS}",
    ])

    make_script(check_name, [
        'echo "=== ruff check: ${TARGETS} ==="',
        "bazel test --config=ruff ${TARGETS}",
    ])
