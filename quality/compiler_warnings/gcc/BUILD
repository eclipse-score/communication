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

cc_feature(
    name = "default_flags",
    args = [
        "//quality/compiler_warnings:default_compile_args",
        "//quality/compiler_warnings:default_link_args",
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
        "-Wall",
        "-Werror",
        "-Wtype-limits",  # Helps to achieve MISRA C++:2023 Rule 0-0-2
        "-Wunused-variable",  # Helps to achieve MISRA C++:2023 Rule 0-2-1
        "-Wunused-local-typedefs",  # Helps to achieve MISRA C++:2023 Rule 0-2-3
        "-Wunused-function",  # Helps to achieve MISRA C++:2023 Rule 0-2-4
        "-Wtrigraphs",  # Helps to achieve MISRA C++:2023 Rule 5-0-1
        "-Wcomment",  # Helps to achieve MISRA C++:2023 Rule 5-7-1 and 5-7-3
        "-Wparentheses",  # Helps to achieve MISRA C++:2023 Rule 6-0-1, 8-0-1, and 8-18-2
        "-Wshadow",  # Helps to achieve MISRA C++:2023 Rule 6-4-1
        "-Wreturn-local-addr",  # Helps to achieve MISRA C++:2023 Rule 6-8-2
        "-Wswitch-bool",  # Helps to achieve MISRA C++:2023 Rule 7-0-1
        "-Wsign-compare",  # Helps to achieve MISRA C++:2023 Rule 7-0-5
        # Enable once we have no warnings
        # "-Wconversion",  # Helps to achieve MISRA C++:2023 Rule 7-0-5 and 7-0-6
        "-Wfloat-conversion",  # Helps to achieve MISRA C++:2023 Rule 7-0-6
        "-Waddress",  # Helps to achieve MISRA C++:2023 Rule 7-11-3
        "-Wcast-function-type",  # Helps to achieve MISRA C++:2023 Rule 8-2-4
        "-Wdangling-else",  # Helps to achieve MISRA C++:2023 Rule 9-3-1
        "-Wmisleading-indentation",  # Helps to achieve MISRA C++:2023 Rule 9-3-1
        "-Wreturn-type",  # Helps to achieve MISRA C++:2023 Rule 9-6-5
        "-Wignored-qualifiers",  # Helps to achieve MISRA C++:2023 Rule 10-1-2
        "-Wenum-compare",  # Helps to achieve MISRA C++:2023 Rule 10-2-3
        "-Wuninitialized",  # Helps to achieve MISRA C++:2023 Rule 11-6-2
        "-Wexpansion-to-defined",  # Helps to achieve MISRA C++:2023 Rule 19-1-1
        "-Wno-error=deprecated-declarations",
        "-Wno-error=cpp",
        "-Wno-builtin-macro-redefined",
        "-Wno-format-y2k",
        "-Wno-free-nonheap-object",
        "-Wno-maybe-uninitialized",
        "-Wno-unused-macros",
        "-Wunused-parameter",
        "-Wunused-variable",
        "-Wunused-but-set-parameter",
    ],
)

cc_args(
    name = "minimal_warnings_args_for_cpp_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions",
    ],
    args = [
        "-Wterminate",  # Helps to achieve MISRA C++:2023 Rule 18-5-1
        "-Wdelete-incomplete",  # Helps to achieve MISRA C++:2023 Rule 21-6-5
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
    name = "strict_warnings_no_error_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Targeted demotions only; -Werror is prepended in the feature below so
        # these reliably take effect.
        "-Wno-error=address-of-packed-member",
        "-Wno-error=alloc-size-larger-than",
        "-Wno-error=array-bounds",
        "-Wno-error=attributes",
        "-Wno-error=conversion",
        "-Wno-error=dangling-pointer",
        "-Wno-error=invalid-memory-model",
        "-Wno-error=maybe-uninitialized",
        "-Wno-error=misleading-indentation",
        "-Wno-error=missing-attributes",
        "-Wno-error=nonnull",
        "-Wno-error=pessimizing-move",
        "-Wno-error=range-loop-construct",
        "-Wno-error=redundant-move",
        "-Wno-error=return-type",
        "-Wno-error=shadow",
        "-Wno-error=sign-compare",
        "-Wno-error=sign-conversion",
        "-Wno-error=stringop-overflow",
        "-Wno-error=stringop-truncation",
        "-Wno-error=strict-aliasing",
        # GCC-only ThreadSanitizer instrumentation diagnostic (enabled by default);
        # kept non-fatal so it cannot break --config=tsan builds. Clang has no -Wtsan.
        "-Wno-error=tsan",
        "-Wno-error=type-limits",
        "-Wno-error=uninitialized",
        "-Wno-error=unused-but-set-variable",
        "-Wno-error=unused-function",
    ],
)

cc_feature(
    name = "strict_warnings_no_error",
    args = [
        "//quality/compiler_warnings:treat_warnings_as_errors_args",
        ":strict_warnings_no_error_args",
    ],
    feature_name = "score_communication_strict_warnings_no_error",
    implies = ["strict_warnings"],
    visibility = ["//visibility:public"],
)

cc_feature(
    name = "additional_warnings",
    args = [
        "//quality/compiler_warnings:additional_warnings_args",
    ],
    feature_name = "score_communication_additional_warnings",
    implies = [":strict_warnings"],
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
        ":additional_warnings",
        "//quality/compiler_warnings:treat_warnings_as_errors",
        ":minimal_warnings",
        ":strict_warnings",
    ],
    visibility = ["//visibility:public"],
)
