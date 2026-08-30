# *******************************************************************************
# Copyright (c) 2024 Contributors to the Eclipse Foundation
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

load("@aspect_rules_lint//format:defs.bzl", "format_multirun", "format_test")
load("@rules_python//python:pip.bzl", "compile_pip_requirements")
load("@rules_python//sphinxdocs:sphinx_docs_library.bzl", "sphinx_docs_library")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load("@score_tooling//cr_checker:cr_checker.bzl", "copyright_checker")
load("@score_tooling//skills_sync:sync_skills.bzl", "sync_skills")
load("//tools/lint:linters.bzl", "use_clang_tidy_targets", "use_ruff_targets")

exports_files(["MODULE.bazel"])

sync_skills()

# Kept in the root package (rather than a .github/BUILD file) so that this
# package doesn't become its own Bazel package: a BUILD file under .github
# would create a package boundary there, causing the sync_skills.check
# glob(".github/skills/score-*/**") above to silently stop matching any
# files and always report the score_tooling skills as missing.
filegroup(
    name = "review_checklists_config",
    srcs = [".github/review_checklists.yml"],
    visibility = ["//tools/review-checklists:__subpackages__"],
)

sphinx_docs_library(
    name = "contributing_md",
    srcs = ["CONTRIBUTING.md"],
    visibility = ["//docs/sphinx:__pkg__"],
)

compile_pip_requirements(
    name = "pip_requirements",
    src = "requirements.in",
    data = [
        "//quality/integration_testing:pip_requirements",
    ],
    exec_compatible_with = ["@platforms//os:linux"],
    requirements_txt = "requirements_lock.txt",
    target_compatible_with = ["@platforms//os:linux"],
)

copyright_checker(
    name = "copyright",
    srcs = [
        ".github",
        "quality",
        "score",
        "third_party",
        "tools",
        "//:BUILD",
        "//:MODULE.bazel",
    ],
    config = "//third_party/cr_checker:config",
    template = "//third_party/cr_checker:templates",
    visibility = ["//:__pkg__"],
)

exports_files([
    ".clang-tidy",
    ".ruff.toml",
])

format_multirun(
    name = "format",
    cc = "@clang_format//:executable",
    python = "@aspect_rules_lint//lint:ruff_bin",
    starlark = "@buildifier_prebuilt//:buildifier",
    target_compatible_with = ["@platforms//os:linux"],
)

format_test(
    name = "format_test",
    cc = "@clang_format//:executable",
    no_sandbox = True,
    python = "@aspect_rules_lint//lint:ruff_bin",
    starlark = "@buildifier_prebuilt//:buildifier",
    tags = ["no-flaky-test-detection"],
    target_compatible_with = ["@platforms//os:linux"],
    workspace = "//:LICENSE",
)

use_clang_tidy_targets()

use_ruff_targets()

sh_binary(
    name = "clang-tidy.fix",
    srcs = [":clang-tidy.fix_script"],
    target_compatible_with = ["@platforms//os:linux"],
)

sh_binary(
    name = "clang-tidy.check",
    srcs = [":clang-tidy.check_script"],
    target_compatible_with = ["@platforms//os:linux"],
)

sh_binary(
    name = "ruff.fix",
    srcs = [":ruff.fix_script"],
    target_compatible_with = ["@platforms//os:linux"],
)

sh_binary(
    name = "ruff.check",
    srcs = [":ruff.check_script"],
    target_compatible_with = ["@platforms//os:linux"],
)
