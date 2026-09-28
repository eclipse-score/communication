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
    name = "default_link_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:link_actions",
    ],
    args = [
        "-fuse-ld=gold",
    ],
)

cc_feature(
    name = "default_flags",
    args = [
        "//quality/compiler_warnings:default_compile_args",
        "//quality/compiler_warnings:default_link_args",
        ":default_link_args",
    ],
    feature_name = "score_communication_gcc_default_flags",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "minimal_warnings_args_for_all_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wno-builtin-macro-redefined",
        # Reduce known GCC false positives.
        "-Wno-maybe-uninitialized",
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
        ":minimal_warnings_args_for_all_compile_actions",
        ":minimal_warnings_args_for_cpp_compile_actions",
    ],
    feature_name = "score_communication_minimal_warnings",
    implies = ["//quality/compiler_warnings:minimal_warnings"],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "strict_warnings_args_for_all_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Warray-bounds=2",  # A5-2-5
        "-Wdisabled-optimization",
        "-Wimplicit-fallthrough=4",
        "-Wmissing-format-attribute",
        "-Wscalar-storage-order",
        "-Wsuggest-attribute=format",
        "-Wvector-operation-performance",
        "-Wlogical-op",
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
        ":strict_warnings_args_for_all_compile_actions",
        ":strict_warnings_args_for_cpp_compile_actions",
        ":strict_warnings_args_for_c_compile_actions",
    ],
    feature_name = "score_communication_strict_warnings",
    implies = [
        "//quality/compiler_warnings:strict_warnings",
        "minimal_warnings",
    ],
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
        "//quality/compiler_warnings:treat_warnings_as_errors",
        ":minimal_warnings",
        ":strict_warnings",
    ],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "misra_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions",
    ],
    # This contains the list of warnings that we decided to enable for C++ to help fulfilling
    # MISRA C++:2023. This list contains only the warnings that are enabled in gcc only.
    # The warnings that are common can be found in the BUILD file one level up.
    args = [
        "-Wunused-local-typedefs",  # Helps to achieve MISRA C++:2023 Rule 0-2-3
        "-Wreturn-local-addr",  # Helps to achieve MISRA C++:2023 Rule 6-8-2
        "-Wfloat-conversion",  # Helps to achieve MISRA C++:2023 Rule 7-0-6
        "-Wcast-function-type",  # Helps to achieve MISRA C++:2023 Rule 8-2-4
        "-Wterminate",  # Helps to achieve MISRA C++:2023 Rule 18-5-1
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
