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

load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")

cc_library(
    name = "non_allocating_future",
    srcs = [
        "non_allocating_future.cpp",
    ],
    hdrs = [
        "non_allocating_future.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    tags = ["FFI"],
    visibility = ["//visibility:public"],
)

cc_gtest_unit_test(
    name = "non_allocating_future_test",
    srcs = [
        "non_allocating_future_samples_test.cpp",
        "non_allocating_future_unit_test.cpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":non_allocating_future",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":non_allocating_future_test",
    ],
    visibility = ["//visibility:public"],
)
