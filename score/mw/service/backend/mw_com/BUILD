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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "single_instantiation_strategy",
    srcs = ["single_instantiation_strategy.cpp"],
    hdrs = ["single_instantiation_strategy.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        ":proxy_holder",
        "//score/mw/service:proxy_builder_base",
        "//score/mw/service/backend/common:instantiation_strategy_base",
        "//score/mw/service/backend/common:port_identifier_checking",
        "@score_baselibs//score/mw/log",
    ],
)

cc_library(
    name = "multiple_instantiation_strategy",
    srcs = ["multiple_instantiation_strategy.cpp"],
    hdrs = ["multiple_instantiation_strategy.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        ":proxy_holder",
        "//score/mw/service:proxy_builder_base",
        "//score/mw/service/backend/common:instantiation_strategy_base",
        "//score/mw/service/backend/common:port_identifier_checking",
    ],
)

cc_library(
    name = "proxy_holder",
    srcs = ["proxy_holder.cpp"],
    hdrs = ["proxy_holder.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
        "@score_communication//score/mw/com",
    ],
)

cc_library(
    name = "proxy_stub",
    testonly = True,
    srcs = ["proxy_stub.cpp"],
    hdrs = ["proxy_stub.h"],
    deps = [
        "@googletest//:gtest",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
        "@score_communication//score/mw/com",
    ],
)

cc_test(
    name = "proxy_holder_test",
    timeout = "long",  # due to long execution time when run in conj. with `memcheck`
    srcs = ["proxy_holder_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["unit"],
    deps = [
        ":proxy_holder",
        ":proxy_stub",
        "@googletest//:gtest_main",
        "@score_baselibs//score/concurrency:notification",
        "@score_baselibs//score/language/safecpp/coverage_termination_handler",
    ],
)

cc_gtest_unit_test(
    name = "proxy_holder_robustness_test",
    srcs = ["proxy_holder_robustness_test.cpp"],
    deps = [
        "//score/mw/service",
        "//score/mw/service:factory",
        "//score/mw/service/backend/mw_com:proxy_holder",
        "//score/mw/service/backend/mw_com:proxy_stub",
        "//score/mw/service/backend/mw_com:single_instantiation_strategy",
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/concurrency/future",
    ],
)

cc_gtest_unit_test(
    name = "single_instantiation_strategy_test",
    srcs = ["single_instantiation_strategy_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":proxy_stub",
        ":single_instantiation_strategy",
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/concurrency/future",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
        "@score_communication//score/mw/com",
    ],
)

cc_gtest_unit_test(
    name = "multiple_instantiation_strategy_test",
    srcs = ["multiple_instantiation_strategy_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":multiple_instantiation_strategy",
        ":proxy_stub",
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/concurrency/future",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
        "@score_communication//score/mw/com",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":single_instantiation_strategy_test",
        ":multiple_instantiation_strategy_test",
        ":proxy_holder_test",
        ":proxy_holder_robustness_test",
    ],
    visibility = ["//score/mw/service/backend:__pkg__"],
)
