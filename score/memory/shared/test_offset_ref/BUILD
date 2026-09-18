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

load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_unit_test(
    name = "offset_ref_test",
    srcs = [
        "offset_ref_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        "//score/memory/shared:offset_ref",
        "//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "offset_ref_precondition_violation_test",
    srcs = [
        "offset_ref_bounds_check_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # This test catches exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        "//score/memory/shared:offset_ref",
        "//score/memory/shared/fake:fake_memory_resources",
        "//score/memory/shared/test_offset_ptr:bounds_check_memory_pool",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)
