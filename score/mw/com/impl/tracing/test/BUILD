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
load("@score-baselibs//bazel:unit_tests.bzl", "cc_gtest_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

package(default_visibility = [
    # "@ddad//score/mw/com/impl/tracing/test:__subpackages__",
])

cc_library(
    name = "runtime_mock_guard",
    testonly = True,
    srcs = ["runtime_mock_guard.cpp"],
    hdrs = ["runtime_mock_guard.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:runtime_mock",
    ],
)

cc_gtest_unit_test(
    name = "proxy_event_tracing_test",
    srcs = ["proxy_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":runtime_mock_guard",
        "//score/mw/com/impl",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/test:binding_factory_resources",
        "//score/mw/com/impl/test:proxy_resources",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_event_tracing_test",
    srcs = ["proxy_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":runtime_mock_guard",
        "//score/mw/com/impl",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/test:binding_factory_resources",
        "//score/mw/com/impl/test:proxy_resources",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_field_tracing_test",
    srcs = ["proxy_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":runtime_mock_guard",
        "//score/mw/com/impl",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/test:binding_factory_resources",
        "//score/mw/com/impl/test:proxy_resources",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_tracing_test",
    srcs = ["proxy_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":runtime_mock_guard",
        "//score/mw/com/impl",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/test:binding_factory_resources",
        "//score/mw/com/impl/test:proxy_resources",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
    ],
)

test_suite(
    name = "unit_test_suite",
    tests = [
        ":proxy_event_tracing_test",
        ":skeleton_event_tracing_test",
        ":skeleton_field_tracing_test",
        ":skeleton_tracing_test",
    ],
    visibility = ["//score/mw/com/impl/tracing:__pkg__"],
)
