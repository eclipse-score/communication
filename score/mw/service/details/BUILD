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
    name = "details",
    srcs = [
        "create_proxies.cpp",
        "proxy_builder.cpp",
        "proxy_needs_factory_checker.cpp",
        "tuple_index_for_type.cpp",
        "type_holder.cpp",
    ],
    hdrs = [
        "create_proxies.h",
        "proxy_builder.h",
        "proxy_needs_factory_checker.h",
        "tuple_index_for_type.h",
        "type_holder.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],  # private library
    visibility = ["//score/mw/service:__subpackages__"],
    deps = [
        "//score/mw/service:proxy_builder_base",
        "//score/mw/service:proxy_future",
        "@score_baselibs//score/concurrency/future",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_spec_traits",
    srcs = ["proxy_spec_traits.cpp"],
    hdrs = ["proxy_spec_traits.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],  # private library
    visibility = ["//score/mw/service:__subpackages__"],
    deps = [
        "//score/mw/service:multi_instance_holder",
        "//score/mw/service:proxy_data",
        "//score/mw/service:proxy_future",
        "@score_baselibs//score/concurrency/future",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "proxy_builder_test",
    srcs = ["proxy_builder_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":details",
        "//score/mw/service:find_service_strategy",
        "//score/mw/service/test_doubles",
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_test(
    name = "proxy_spec_traits_test",
    srcs = ["proxy_spec_traits_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "throws_upon_exception",
    ],
    tags = ["unit"],
    deps = [
        ":details",
        ":proxy_spec_traits",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/language/safecpp/coverage_termination_handler",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":proxy_spec_traits_test",
        ":proxy_builder_test",
    ],
    visibility = ["//score/mw/service:__pkg__"],
)
