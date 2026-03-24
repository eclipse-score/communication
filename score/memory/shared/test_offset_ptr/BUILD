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
load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_gtest_unit_test(
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
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":offset_ptr_test_resources",
        "@score_baselibs//score/memory/shared:offset_ptr",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_library(
    name = "offset_ptr_test_resources",
    testonly = True,
    srcs = ["offset_ptr_test_resources.cpp"],
    hdrs = ["offset_ptr_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["@score_baselibs//score/memory/shared:__pkg__"],
    deps = [
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory/shared:managed_memory_resource",
        "@score_baselibs//score/memory/shared:offset_ptr",
        "@score_baselibs//score/memory/shared/fake:fake_memory_resources",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":offset_ptr_test",
    ],
    visibility = [
        "@score_baselibs//score/memory/shared:__pkg__",
    ],
)
