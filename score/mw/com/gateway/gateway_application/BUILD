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

package(
    default_visibility = ["//score/mw/com/gateway:__subpackages__"],
)

cc_library(
    name = "gateway_application",
    srcs = ["gateway_application.cpp"],
    hdrs = ["gateway_application.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        ":gateway_error",
        "//score/mw/com/gateway/transport_layer:transport_factory",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
    ],
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        ":gateway_core",
        "//score/mw/com/gateway/gateway_application/configuration:gateway_configuration",
        "//score/mw/com/gateway/transport_layer:transport",
        "//score/mw/com/impl:find_service_handle",
        "//score/mw/com/impl:generic_proxy",
        "//score/mw/com/impl:generic_skeleton",
        "//score/mw/com/impl:instance_specifier",
        "@score_baselibs//score/language/safecpp/scoped_function:move_only_scoped_function",
        "@score_baselibs//score/language/safecpp/scoped_function:scope",
    ],
)

cc_library(
    name = "gateway_core",
    hdrs = ["gateway_core.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        "//score/mw/com/gateway/transport_layer:transport",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl:service_element_type",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "gateway_error",
    srcs = ["gateway_error.cpp"],
    hdrs = ["gateway_error.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = ["@score_baselibs//score/result"],
)

cc_unit_test(
    name = "gateway_error_test",
    srcs = ["gateway_error_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":gateway_error",
        "@score_baselibs//score/result",
    ],
)

cc_unit_test(
    name = "gateway_application_test",
    srcs = ["gateway_application_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":gateway_application",
        ":gateway_error",
        "//score/mw/com/gateway/gateway_application/configuration:gateway_configuration",
        "//score/mw/com/gateway/transport_layer:transport_mock",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl:service_element_type",
        "@googletest//:gtest",
        "@score_baselibs//score/result",
    ],
)
