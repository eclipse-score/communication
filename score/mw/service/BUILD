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
load("@score_baselibs//score/quality/clang_tidy:extra_checks.bzl", "clang_tidy_extra_checks")

# To be referenced by the application core logic
cc_library(
    name = "service",
    srcs = [
        "get_proxy.cpp",
        "proxy_container.cpp",
        "proxy_needs.cpp",
    ],
    hdrs = [
        "get_proxy.h",
        "proxy_container.h",
        "proxy_needs.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        ":multi_instance_holder",
        ":proxy_future",
        "//score/mw/service/details",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
    ],
)

# To be referenced by the application factory
cc_library(
    name = "factory",
    srcs = ["proxy_needs_factory.cpp"],
    hdrs = ["proxy_needs_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        ":service",
        "//score/mw/service/details",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "managed_service",
    srcs = ["managed_service.cpp"],
    hdrs = ["managed_service.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "provided_services_container",
    srcs = ["provided_services_container.cpp"],
    hdrs = ["provided_services_container.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        ":managed_service",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "single_instance_holder",
    srcs = ["single_instance_holder.cpp"],
    hdrs = ["single_instance_holder.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "multi_instance_holder",
    srcs = ["multi_instance_holder.cpp"],
    hdrs = ["multi_instance_holder.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        ":single_instance_holder",
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/language/futurecpp",
    ],
)

# Only to be dependent on custom strategies
cc_library(
    name = "proxy_builder_base",
    srcs = ["proxy_builder_base.cpp"],
    hdrs = ["proxy_builder_base.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        ":find_service_strategy",
        ":proxy_future",
        "//score/mw/service/details:proxy_spec_traits",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "find_service_strategy",
    srcs = ["find_service_strategy.cpp"],
    hdrs = ["find_service_strategy.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],  # platform_only
    deps = [],
)

cc_library(
    name = "proxy_future",
    srcs = ["proxy_future.cpp"],
    hdrs = ["proxy_future.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        "@score_baselibs//score/concurrency/future",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/safecpp/scoped_function:move_only_scoped_function",
    ],
)

cc_library(
    name = "proxy_data",
    hdrs = ["proxy_data.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        ":multi_instance_holder",
        ":proxy_future",
        ":single_instance_holder",
    ],
)

cc_library(
    name = "find_service_strategy_mock",
    testonly = True,
    srcs = ["find_service_strategy_mock.cpp"],
    hdrs = ["find_service_strategy_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        ":find_service_strategy",
        "@googletest//:gtest",
    ],
)

cc_gtest_unit_test(
    name = "single_instance_holder_test",
    srcs = ["single_instance_holder_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":single_instance_holder",
    ],
)

cc_gtest_unit_test(
    name = "multi_instance_holder_test",
    srcs = ["multi_instance_holder_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [":multi_instance_holder"],
)

cc_gtest_unit_test(
    name = "provided_services_container_test",
    srcs = ["provided_services_container_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":provided_services_container",
        "@score_baselibs//score/result",
    ],
)

cc_gtest_unit_test(
    name = "proxy_container_test",
    srcs = ["proxy_container_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":service",
        "//score/mw/service/test_doubles",
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/concurrency/future",
    ],
)

cc_gtest_unit_test(
    name = "proxy_future_test",
    srcs = ["proxy_future_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":proxy_future",
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/concurrency/future",
    ],
)

cc_gtest_unit_test(
    name = "proxy_needs_factory_test",
    srcs = ["proxy_needs_factory_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":factory",
        "//score/mw/service/test_doubles",
    ],
)

cc_gtest_unit_test(
    name = "proxy_needs_test",
    srcs = ["proxy_needs_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":factory",
        ":find_service_strategy",
        ":service",
        "//score/mw/service/test_doubles",
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/concurrency/future",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":single_instance_holder_test",
        ":multi_instance_holder_test",
        ":provided_services_container_test",
        ":proxy_container_test",
        ":proxy_future_test",
        ":proxy_needs_factory_test",
        ":proxy_needs_test",
    ],
    test_suites_from_sub_packages = [
        "//score/mw/service/backend:unit_test_suite",
        "//score/mw/service/details:unit_test_suite",
        "//score/mw/service/utils:unit_test_suite",
    ],
    visibility = ["//score/mw:__pkg__"],
)

clang_tidy_extra_checks(
    name = "clang_tidy_extra_checks",
    tidy_config_file = ".clang-tidy-extra",
)
