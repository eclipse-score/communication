# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

filegroup(
    name = "repo_suppressions",
    srcs = [
        "asan.supp",
        "lsan.supp",
        "tsan.supp",
        "ubsan.supp",
    ],
    visibility = ["//quality/integration_testing/sanitizer_support:__pkg__"],
)

# ==============================================================================
# Canary tests — verify sanitizer instrumentation is actually active
# ==============================================================================

cc_binary(
    name = "asan_canary_bin",
    testonly = True,
    srcs = ["asan_canary.cpp"],
    target_compatible_with = ["@score_cpp_policies//sanitizers/constraints:no_tsan"],
)

sh_test(
    name = "asan_canary",
    srcs = ["verify_sanitizer_failure.sh"],
    args = [
        "$(location :asan_canary_bin)",
        "55",
    ],
    data = [":asan_canary_bin"],
    target_compatible_with = [
        "@score_cpp_policies//sanitizers/constraints:no_tsan",
        "@score_cpp_policies//sanitizers/constraints:any_sanitizer",
    ],
)

cc_binary(
    name = "tsan_canary_bin",
    testonly = True,
    srcs = ["tsan_canary.cpp"],
    target_compatible_with = ["@score_cpp_policies//sanitizers/constraints:no_asan_ubsan_lsan"],
)

sh_test(
    name = "tsan_canary",
    srcs = ["verify_sanitizer_failure.sh"],
    args = [
        "$(location :tsan_canary_bin)",
        "55",
    ],
    data = [":tsan_canary_bin"],
    target_compatible_with = [
        "@score_cpp_policies//sanitizers/constraints:no_asan_ubsan_lsan",
        "@score_cpp_policies//sanitizers/constraints:any_sanitizer",
    ],
)

# ------------------------------------------------------------------------------
# Wildcard canary aliases so --config=gcc_asan_ubsan_lsan / --config=gcc_tsan
# can target the same canary names that the LLVM configs target.
# These use the same underlying binaries/sh_tests; only the toolchain differs.
# ------------------------------------------------------------------------------

# ==============================================================================
# GCC-compatible sanitizer features
# ==============================================================================
# The @score_cpp_policies//sanitizers/features include -fsanitize-link-c++-runtime
# which is Clang-only. These define equivalent features without that flag.

cc_args(
    name = "gcc_asan_ubsan_lsan_link_args",
    actions = ["@rules_cc//cc/toolchains/actions:link_actions"],
    args = ["-fsanitize=undefined,address,leak"],
)

cc_feature(
    name = "gcc_asan_ubsan_lsan_feature",
    args = [
        "@score_cpp_policies//sanitizers/features:asan_ubsan_lsan_compile_args",
        ":gcc_asan_ubsan_lsan_link_args",
    ],
    feature_name = "score_asan_ubsan_lsan",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "gcc_tsan_link_args",
    actions = ["@rules_cc//cc/toolchains/actions:link_actions"],
    args = ["-fsanitize=thread"],
)

cc_feature(
    name = "gcc_tsan_feature",
    args = [
        "@score_cpp_policies//sanitizers/features:tsan_compile_args",
        ":gcc_tsan_link_args",
    ],
    feature_name = "score_tsan",
    visibility = ["//visibility:public"],
)
