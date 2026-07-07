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

# Common compiler warning features shared across GCC and Clang.

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

cc_args(
    name = "default_compile_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-march=nehalem",
        "-ffp-model=strict",
        # Undefine _FORTIFY_SOURCE to avoid conflict with sanitizers.
        # See https://github.com/google/sanitizers/issues/247
        "-U_FORTIFY_SOURCE",
        "-fstack-protector",
        "-fno-omit-frame-pointer",
        "-fcolor-diagnostics",
    ],
)

cc_feature(
    name = "default_flags",
    args = [
        ":default_compile_args",
    ],
    feature_name = "score_communication_default_flags",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "default_link_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:link_actions",
    ],
    args = [
        "-lrt",
        # Required for targets using std::atomic.
        "-latomic",
    ],
)

cc_feature(
    name = "default_link_flags",
    args = [
        ":default_link_args",
    ],
    feature_name = "score_communication_default_link_flags",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "treat_warnings_as_errors_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Werror",
    ],
)

cc_feature(
    name = "treat_warnings_as_errors",
    args = [
        ":treat_warnings_as_errors_args",
    ],
    feature_name = "score_communication_treat_warnings_as_errors",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "minimal_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wall",
        # Keep #warning visible without failing the build.
        "-Wno-error=cpp",
        "-Wno-error=deprecated-declarations",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

cc_args(
    name = "strict_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wextra",
        "-Wpedantic",
        "-Wconversion",
        "-Wsign-conversion",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

cc_args(
    name = "additional_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wcast-align",
        "-Wcast-qual",
        "-Wdouble-promotion",
        "-Wformat-nonliteral",
        "-Wpointer-arith",
        "-Wredundant-decls",
        "-Wundef",
        "-Wwrite-strings",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

cc_feature(
    name = "additional_warnings",
    args = [
        ":additional_warnings_args",
    ],
    feature_name = "score_communication_additional_warnings",
    visibility = ["//visibility:public"],
)
