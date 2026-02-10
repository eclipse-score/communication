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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "api",
    hdrs = glob(["api_under_test/**/*.h"]),
    features = COMPILER_WARNING_FEATURES,
    strip_include_prefix = "api_under_test",
    deps = [
        "//score/mw/com",
        "@score_baselibs//score/memory/shared:types",
    ],
)

cc_gtest_unit_test(
    name = "generated_api_test",
    srcs = ["generated_api_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":api",
        "@score_logging//score/mw/log",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":generated_api_test",
    ],
    visibility = ["//score/mw/com:__pkg__"],
)
