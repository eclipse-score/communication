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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

package(
    default_visibility = ["//score/mw/com:__subpackages__"],
)

cc_library(
    name = "serialization_request",
    srcs = ["serialization_request.cpp"],
    hdrs = ["serialization_request.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:instance_specifier",
    ],
)

cc_test(
    name = "serialization_request_test",
    size = "small",
    timeout = "short",
    srcs = ["serialization_request_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":serialization_request",
        "//score/mw/com/impl:instance_specifier",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "serialization_infrastructure",
    srcs = ["serialization_infrastructure.cpp"],
    hdrs = ["serialization_infrastructure.h"],
    features = COMPILER_WARNING_FEATURES,
    linkopts = ["-ldl"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:error",
        "@score_baselibs//score/os:dlfcn",
        "@score_baselibs//score/result",
    ],
)

cc_binary(
    name = "serialization_infrastructure_test_plugin.so",
    srcs = ["serialization_infrastructure_test_plugin.cpp"],
    features = COMPILER_WARNING_FEATURES,
    linkshared = True,
)

cc_test(
    name = "serialization_infrastructure_integration_test",
    size = "small",
    timeout = "short",
    srcs = ["serialization_infrastructure_integration_test.cpp"],
    data = [":serialization_infrastructure_test_plugin.so"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":serialization_infrastructure",
        "@googletest//:gtest_main",
        "@score_baselibs//score/os/mocklib:dlfcn_mock",
    ],
)
