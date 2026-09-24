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
    name = "instantiation_strategy_base",
    srcs = ["instantiation_strategy_base.cpp"],
    hdrs = ["instantiation_strategy_base.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/service/backend:__subpackages__",
    ],
    deps = [
        "//score/mw/service:find_service_strategy",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "port_identifier_checking",
    srcs = ["port_identifier_checking.cpp"],
    hdrs = ["port_identifier_checking.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/service/backend:__subpackages__",
    ],
)

cc_gtest_unit_test(
    name = "port_identifier_checking_test",
    srcs = ["port_identifier_checking_test.cpp"],
    deps = [
        ":port_identifier_checking",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":port_identifier_checking_test",
    ],
    test_suites_from_sub_packages = [
        "//score/mw/service/backend/common/test:unit_test_suite",
    ],
    visibility = [
        "//score/mw/service/backend:__pkg__",
    ],
)
