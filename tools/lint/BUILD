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

load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load("//bazel/rules:expand_template.bzl", "expand_template")

# Makes tools/lint/ a Bazel package so linters.bzl can be referenced as a
# label target (e.g. //tools/lint:linters.bzl%clang_tidy) in bazelrc.
exports_files([
    "linters.bzl",
])

# Wraps @score_rust_policies//rustfmt:rustfmt.toml (a plain exports_files
# source file, which //bazel/rules:expand_template.bzl's `deps` attribute
# rejects) in a filegroup so it can be depended on for location expansion.
filegroup(
    name = "rustfmt_toml",
    srcs = ["@score_rust_policies//rustfmt:rustfmt.toml"],
)

# Generates the rustfmt_with_config wrapper script by substituting the
# runfiles ("rlocation") paths of the S-CORE rustfmt policy config and the
# rules_rust rustfmt binary into the template. See rustfmt_wrapper.sh.tpl for
# why this indirection exists. //bazel/rules:expand_template.bzl is used
# (rather than @bazel_skylib's) because it expands the `$(rlocationpath ...)`
# label expressions below via `ctx.expand_location`, which the skylib rule
# does not support.
expand_template(
    name = "rustfmt_with_config_script",
    out = "rustfmt_wrapper.sh",
    expression_substitutions = {
        "@@RUSTFMT_BIN@@": "$(rlocationpath @rules_rust//tools/upstream_wrapper:rustfmt)",
        "@@RUSTFMT_TOML@@": "$(rlocationpath :rustfmt_toml)",
    },
    is_executable = True,
    template = "rustfmt_wrapper.sh.tpl",
    deps = [
        ":rustfmt_toml",
        "@rules_rust//tools/upstream_wrapper:rustfmt",
    ],
)

# A drop-in replacement for `@rules_rust//tools/upstream_wrapper:rustfmt`
# that always formats/checks using the shared S-CORE rustfmt policy pulled in
# via the `score_rust_policies` bazel_dep (see MODULE.bazel), rather than a
# rustfmt.toml checked into this repository.
sh_binary(
    name = "rustfmt_with_config",
    srcs = [":rustfmt_with_config_script"],
    data = [
        ":rustfmt_toml",
        "@rules_rust//tools/upstream_wrapper:rustfmt",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//:__pkg__"],
    deps = ["@bazel_tools//tools/bash/runfiles"],
)
