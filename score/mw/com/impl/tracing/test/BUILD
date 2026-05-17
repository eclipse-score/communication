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

package(default_visibility = [
    "//score/mw/com/impl/tracing/test:__subpackages__",
])

cc_unit_test(
    name = "proxy_event_tracing_test",
    srcs = ["proxy_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/impl",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/test:binding_factory_resources",
        "//score/mw/com/impl/test:proxy_resources",
        "//score/mw/com/impl/test:runtime_mock_guard",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
    ],
)

cc_unit_test(
    name = "skeleton_event_tracing_test",
    srcs = ["proxy_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/impl",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/test:binding_factory_resources",
        "//score/mw/com/impl/test:proxy_resources",
        "//score/mw/com/impl/test:runtime_mock_guard",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
    ],
)

cc_unit_test(
    name = "skeleton_field_tracing_test",
    srcs = ["proxy_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/impl",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/test:binding_factory_resources",
        "//score/mw/com/impl/test:proxy_resources",
        "//score/mw/com/impl/test:runtime_mock_guard",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
    ],
)

cc_unit_test(
    name = "skeleton_tracing_test",
    srcs = ["proxy_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/impl",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/test:binding_factory_resources",
        "//score/mw/com/impl/test:proxy_resources",
        "//score/mw/com/impl/test:runtime_mock_guard",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
    ],
)
