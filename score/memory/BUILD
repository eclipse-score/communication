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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("@score_baselibs//:bazel/unit_tests.bzl", "cc_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("@score_baselibs//score/quality/clang_tidy:extra_checks.bzl", "clang_tidy_extra_checks")

package(default_visibility = [
    "//score/memory:__subpackages__",
])

alias(
    name = "string_literal",
    actual = "@score_baselibs//score/string:string_literal",
    deprecation = "Use @score_baselibs//score/string:string_literal directly. This alias will be removed.",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "endianness",
    srcs = ["endianness.cpp"],
    hdrs = ["endianness.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//platform/aas/lib/serialization:__subpackages__",
        "//platform/aas/pas/sysmon/library:__subpackages__",
    ],
    deps = ["@score_baselibs//score/language/futurecpp"],
)

alias(
    name = "string_comparison_adaptor",
    actual = "@score_baselibs//score/string:string_comparison_adaptor",
    deprecation = "Use @score_baselibs//score/string:string_comparison_adaptor directly. This alias will be removed.",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "data_type_size_info",
    srcs = ["data_type_size_info.cpp"],
    hdrs = ["data_type_size_info.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],
    deps = ["@score_baselibs//score/language/futurecpp"],
)


alias(
    name = "split_string_view",
    actual = "@score_baselibs//score/string:split_string_view",
    deprecation = "Use @score_baselibs//score/string:split_string_view directly. This alias will be removed.",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "pmr_ring_buffer",
    hdrs = [
        "pmr_ring_buffer.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)


cc_test(
    name = "endianness_unit_test",
    srcs = [
        "endianness_test.cpp",
    ],
    features = [
        "aborts_upon_exception",
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    tags = ["unit"],
    deps = [
        ":endianness",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "pmr_ring_buffer_unit_test",
    srcs = [
        "pmr_ring_buffer_test.cpp",
    ],
    features = [
        "aborts_upon_exception",
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    tags = ["unit"],
    deps = [
        ":pmr_ring_buffer",
        "@googletest//:gtest_main",
    ],
)

cc_unit_test(
    name = "data_type_size_info_test",
    srcs = ["data_type_size_info_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        ":data_type_size_info",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":endianness_unit_test",
        ":pmr_ring_buffer_unit_test",
        ":data_type_size_info_test",
    ],
    test_suites_from_sub_packages = [
        "//score/memory/shared:unit_test_suite",
    ],
    visibility = [
        "//visibility:public",
    ],
)

clang_tidy_extra_checks(
    name = "clang_tidy_extra_checks",
    extra_features = [
        "spp_code_style_check_header_guards",
    ],
    tidy_config_file = ".clang-tidy-extra",
)
