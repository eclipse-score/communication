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
    name = "single_immediate_instantiation_strategy",
    testonly = True,
    srcs = ["single_immediate_instantiation_strategy.cpp"],
    hdrs = ["single_immediate_instantiation_strategy.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        "//score/mw/service:proxy_builder_base",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "multiple_immediate_instantiation_strategy",
    testonly = True,
    srcs = ["multiple_immediate_instantiation_strategy.cpp"],
    hdrs = ["multiple_immediate_instantiation_strategy.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        "//score/mw/service:proxy_builder_base",
        "//score/mw/service/details:proxy_spec_traits",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "single_immediate_instantiation_strategy_test",
    srcs = ["single_immediate_instantiation_strategy_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":single_immediate_instantiation_strategy",
    ],
)

cc_gtest_unit_test(
    name = "multiple_immediate_instantiation_strategy_test",
    srcs = ["multiple_immediate_instantiation_strategy_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":multiple_immediate_instantiation_strategy",
    ],
)

cc_gtest_unit_test(
    name = "instantiation_strategy_base_test",
    srcs = ["instantiation_strategy_base_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "throws_upon_exception",
    ],
    deps = [
        "//score/mw/service/backend/common:instantiation_strategy_base",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":instantiation_strategy_base_test",
        ":multiple_immediate_instantiation_strategy_test",
        ":single_immediate_instantiation_strategy_test",
    ],
    visibility = ["//score/mw/service/backend/common:__pkg__"],
)
