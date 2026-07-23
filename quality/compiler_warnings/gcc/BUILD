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
        "-W",
        "-Wno-builtin-macro-redefined",
        "-Wno-deprecated-non-prototype",
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
    name = "strict_warnings_no_error_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Must come after -Werror so it takes effect.
        "-Wno-error",
        "-Wno-error=address-of-packed-member",
        "-Wno-error=alloc-size-larger-than=",
        "-Wno-error=array-bounds",
        "-Wno-error=attributes",
        "-Wno-error=conversion",
        "-Wno-error=dangling-pointer=2",
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
        "-Wno-error=stringop-overflow=2",
        "-Wno-error=stringop-truncation",
        "-Wno-error=strict-aliasing",
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
