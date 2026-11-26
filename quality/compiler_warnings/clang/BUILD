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

# Clang warning features.
# Diagnostics reference: https://clang.llvm.org/docs/DiagnosticsReference.html

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

cc_args(
    name = "minimal_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Warray-bounds",
        "-Wimplicit-fallthrough",
    ],
)

cc_feature(
    name = "minimal_warnings",
    args = [
        ":minimal_warnings_args",
        "//quality/compiler_warnings:minimal_warnings_args",
    ],
    feature_name = "score_communication_minimal_warnings",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "misra_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions",
    ],
    # This contains the list of warnings that we decided to enable for C++ to help fulfilling
    # MISRA C++:2023. This list contains only the warnings that are enabled in clang only.
    # The warnings that are common can be found in the BUILD file one level up.
    args = [
        "-Wdeprecated",  # Helps to achieve MISRA C++:2023 Rule 4-1-2
    ],
)

cc_feature(
    name = "misra_warnings",
    args = [
        ":misra_warnings_args",
        "//quality/compiler_warnings:minimal_warnings_args",
    ],
    feature_name = "score_communication_misra_warnings",
    visibility = ["//visibility:public"],
)
