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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_unit_test(
    name = "offset_ptr_test",
    srcs = [
        "assignment_test.cpp",
        "bool_ops_test.cpp",
        "compare_test.cpp",
        "construction_test.cpp",
        "index_dereference_test.cpp",
        "misc_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":offset_ptr_test_resources",
        "//score/memory/shared:offset_ptr",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "offset_ptr_precondition_violation_test",
    srcs = [
        "arithmetic_test.cpp",
        "bounds_check_test.cpp",
        "copying_offset_ptr_bounds_check_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":bounds_check_memory_pool",
        "//score/memory/shared:offset_ptr",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_library(
    name = "offset_ptr_test_resources",
    testonly = True,
    srcs = ["offset_ptr_test_resources.cpp"],
    hdrs = ["offset_ptr_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/memory/shared:__pkg__"],
    deps = [
        "//score/memory/shared",
        "//score/memory/shared/fake:fake_memory_resources",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "bounds_check_memory_pool",
    testonly = True,
    srcs = ["bounds_check_memory_pool.cpp"],
    hdrs = ["bounds_check_memory_pool.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":offset_ptr_test_resources",
        "//score/memory/shared",
        "@score_baselibs//score/language/futurecpp",
    ],
)
