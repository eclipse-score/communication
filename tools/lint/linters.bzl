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

"""Clang-tidy aspect and developer check/fix targets."""

load("@aspect_rules_lint//lint:clang_tidy.bzl", "lint_clang_tidy_aspect")
load("@aspect_rules_lint//lint:lint_test.bzl", "lint_test")
load("@aspect_rules_lint//lint:pylint.bzl", "lint_pylint_aspect")
load("@bazel_skylib//rules:write_file.bzl", "write_file")

visibility(["//..."])

# Define the clang-tidy linter aspect using LLVM toolchain
clang_tidy = lint_clang_tidy_aspect(
    binary = Label("@llvm_toolchain//:clang-tidy"),
    configs = [
        Label("//:.clang-tidy"),
    ],
    lint_target_headers = True,
    angle_includes_are_system = True,
    verbose = False,
)

# Create a test rule for clang-tidy (for individual targets)
clang_tidy_test = lint_test(aspect = clang_tidy)

# Define the pylint linter aspect for Python targets
pylint = lint_pylint_aspect(
    binary = Label("//tools/lint:pylint"),
    config = Label("//:.pylintrc"),
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

def use_pylint_targets(check_name = "pylint.check"):
    """Declare a pylint check script target.

    Pylint has no auto-fix mode (unlike clang-tidy), so only a check target
    is provided.
    """
    make_script(check_name, [
        'echo "=== pylint check: ${TARGETS} ==="',
        "bazel test --config=pylint ${TARGETS}",
    ])
