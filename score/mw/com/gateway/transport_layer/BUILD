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
    name = "transport",
    srcs = ["transport.cpp"],
    hdrs = ["transport.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl:service_element_type",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "transport_error",
    srcs = ["transport_error.cpp"],
    hdrs = ["transport_error.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = ["@score_baselibs//score/result"],
)

cc_unit_test(
    name = "transport_error_test",
    srcs = ["transport_error_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":transport_error",
        "@score_baselibs//score/result",
    ],
)

cc_unit_test(
    name = "transport_test",
    srcs = ["transport_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":transport",
        "@score_baselibs//score/result",
    ],
)
