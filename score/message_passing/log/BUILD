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

cc_library(
    name = "log",
    srcs = [
        "log.cpp",
        "log_on_timeout.cpp",
        "logging_callback.cpp",
    ],
    hdrs = [
        "log.h",
        "log_on_timeout.h",
        "logging_callback.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    tags = ["FFI"],
    visibility = ["//score:__subpackages__"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "log_test",
    srcs = [
        "log_on_timeout_test.cpp",
        "log_test.cpp",
    ],
    visibility = ["//score:__subpackages__"],
    deps = [
        ":log",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":log_test",
    ],
    visibility = ["//score:__subpackages__"],
)
