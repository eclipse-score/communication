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
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "i_bidirectional_transport",
    hdrs = ["i_bidirectional_transport.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        "//score/mw/com/gateway/transport_layer/sample/messages:gateway_messages",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "bidirectional_transport",
    srcs = ["bidirectional_transport.cpp"],
    hdrs = [
        "bidirectional_transport.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/os:socket",
    ],
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        ":i_bidirectional_transport",
        "//score/mw/com/gateway/transport_layer:transport_error",
        "//score/mw/com/gateway/transport_layer/sample:unique_socket",
        "//score/mw/com/gateway/transport_layer/sample/configuration:hypervisor_socket_configuration",
        "//score/mw/com/gateway/transport_layer/sample/messages:gateway_messages",
        "@score_baselibs//score/concurrency:long_running_threads_container",
        "@score_baselibs//score/os:unistd",
    ],
)

cc_library(
    name = "sample_hypervisor_transport",
    srcs = ["sample_hypervisor_transport.cpp"],
    hdrs = [
        "sample_hypervisor_transport.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/gateway/transport_layer/sample/messages:gateway_messages",
        "//score/mw/com/impl:runtime",
        "@score_baselibs//score/mw/log",
    ],
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        "//score/mw/com/gateway/gateway_application:gateway_core",
        "//score/mw/com/gateway/transport_layer:transport",
        "//score/mw/com/gateway/transport_layer/sample:i_bidirectional_transport",
    ],
)

cc_library(
    name = "unique_socket",
    srcs = ["unique_socket.cpp"],
    hdrs = [
        "unique_socket.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/mw/log",
    ],
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/os:unistd",
    ],
)

cc_unit_test(
    name = "unique_socket_test",
    srcs = ["unique_socket_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        ":unique_socket",
        "@score_baselibs//score/os/mocklib:unistd_mock",
    ],
)

cc_unit_test(
    name = "bidirectional_transport_test",
    srcs = [
        "bidirectional_transport_test.cpp",
        "sample_transport_test_resources.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        ":bidirectional_transport",
        "@score_baselibs//score/os/mocklib:socket_mock",
    ],
)

cc_unit_test(
    name = "sample_hypervisor_transport_test",
    srcs = ["sample_hypervisor_transport_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        ":bidirectional_transport_mock",
        ":sample_hypervisor_transport",
        "//score/mw/com/gateway/gateway_application:gateway_core_mock",
        "//score/mw/com/gateway/transport_layer:transport_error",
        "//score/mw/com/impl/test:dummy_instance_identifier_builder",
        "//score/mw/com/impl/test:runtime_mock_guard",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/mw/log:recorder_mock",
    ],
)

cc_library(
    name = "bidirectional_transport_mock",
    hdrs = ["bidirectional_transport_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        ":i_bidirectional_transport",
        "@googletest//:gtest",
    ],
)
