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

# GCC warning features.

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

cc_args(
    name = "minimal_warnings_args_for_all_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wno-format-y2k",
        "-Wno-free-nonheap-object",
        # Reduce known GCC false positives.
        "-Wno-maybe-uninitialized",
        "-Wunused-but-set-parameter",
    ],
)

cc_args(
    name = "minimal_warnings_args_for_cpp_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions",
    ],
    args = [
        "-Wno-literal-suffix",
        "-Wno-noexcept-type",
    ],
)

cc_feature(
    name = "minimal_warnings",
    args = [
        "//quality/compiler_warnings:minimal_warnings_args",
        ":minimal_warnings_args_for_all_compile_actions",
        ":minimal_warnings_args_for_cpp_compile_actions",
    ],
    feature_name = "score_communication_minimal_warnings",
    visibility = ["//:__pkg__"],
)

cc_args(
    name = "strict_warnings_args_for_all_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Warray-bounds=2",
        "-Wcast-align",
        "-Wcast-qual",
        "-Wdisabled-optimization",
        "-Wfloat-conversion",
        "-Wformat=2",
        "-Wimplicit-fallthrough=4",
        "-Winvalid-pch",
        "-Wmissing-format-attribute",
        "-Wmultichar",
        "-Wpacked",
        "-Wscalar-storage-order",
        "-Wsuggest-attribute=format",
        "-Wundef",
        "-Wunused-macros",
        "-Wvector-operation-performance",
        "-Wwrite-strings",
        "-Wlogical-op",
        "-Wredundant-decls",
        "-Wshadow",
    ],
)

cc_args(
    name = "strict_warnings_args_for_cpp_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions",
    ],
    args = [
        "-Wdelete-non-virtual-dtor",
        "-Woverloaded-virtual",
        "-Wregister",
        "-Wstrict-null-sentinel",
    ],
)

cc_args(
    name = "strict_warnings_args_for_c_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:c_compile_actions",
    ],
    args = [
        "-Wold-style-definition",
        "-Wstrict-prototypes",
    ],
)

cc_feature(
    name = "strict_warnings",
    args = [
        "//quality/compiler_warnings:strict_warnings_args",
        ":strict_warnings_args_for_all_compile_actions",
        ":strict_warnings_args_for_cpp_compile_actions",
        ":strict_warnings_args_for_c_compile_actions",
    ],
    feature_name = "score_communication_strict_warnings",
    implies = ["minimal_warnings"],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "strict_warnings_no_error_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Must come after -Werror so it takes effect.
        "-Wno-error",
    ],
)

cc_feature(
    name = "strict_warnings_no_error",
    args = [
        ":strict_warnings_no_error_args",
    ],
    feature_name = "score_communication_strict_warnings_no_error",
    implies = ["strict_warnings"],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "third_party_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-w",
    ],
)

cc_feature(
    name = "third_party_warnings",
    args = [
        ":third_party_warnings_args",
    ],
    feature_name = "score_communication_third_party_warnings",
    mutually_exclusive = [
        "//quality/compiler_warnings:additional_warnings",
        "//quality/compiler_warnings:treat_warnings_as_errors",
        ":minimal_warnings",
        ":strict_warnings",
    ],
    visibility = ["//visibility:public"],
)
