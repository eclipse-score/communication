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

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")
load("@rules_python//python:defs.bzl", "py_binary")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")

sh_binary(
    name = "generate_coverage_html",
    srcs = ["generate_coverage_html.sh"],
    visibility = ["//visibility:private"],
)

py_binary(
    name = "justify",
    srcs = ["justify.py"],
    deps = [
        "@score_communication_pip//pyyaml",
    ],
)

py_binary(
    name = "effective_coverage",
    srcs = ["effective_coverage.py"],
)

py_binary(
    name = "lcov_to_html",
    srcs = ["lcov_to_html.py"],
    deps = [
        "@score_communication_pip//gcovr",
    ],
)

# These compile time options are required to cover abnormal termination cases. In GCC one can use `__gcc_dump()`, but this does not work with LLVM.
# LLVM provides these compile-time options in combination with a specific profile setting which is enabled in bazel via `LLVM_PROFILE_CONTINUOUS_MODE`.
# See coverage.bazelrc
cc_args(
    name = "runtime_relocation_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
        "@rules_cc//cc/toolchains/actions:link_actions",
    ],
    args = [
        "-mllvm",
        "-runtime-counter-relocation",
    ],
)

cc_feature(
    name = "enable_llvm_coverage_for_death_tests",
    args = [":runtime_relocation_args"],
    feature_name = "enable_llvm_coverage_for_death_tests",
    visibility = ["//visibility:public"],
)
