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

# Example of generator output for the SOME/IP binding. The hand-written ABI (ISerializer interface,
# error type, factory signatures and shared serialization helpers) lives in the parent package; the
# datatype, its serializer and the factory below illustrate what the serializer generator emits.

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

package(
    default_visibility = ["//score/mw/com:__subpackages__"],
)

cc_library(
    name = "speed_rpm",
    hdrs = ["speed_rpm.h"],
    features = COMPILER_WARNING_FEATURES,
)

cc_library(
    name = "speed_rpm_serializer",
    srcs = ["speed_rpm_serializer.cpp"],
    hdrs = ["speed_rpm_serializer.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":speed_rpm",
        "//score/serialization/someip:serialization_helpers",
        "//score/serialization/someip:serializer",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

# Generated SOME/IP serialization library loaded by mw::com via dlopen. The factory translation unit
# provides the extern "C" CreateSerializer / DestroySerializer entry points resolved with dlsym.
cc_binary(
    name = "libmwcom_serializer_someip.so",
    srcs = ["factory.cpp"],
    features = COMPILER_WARNING_FEATURES,
    linkshared = True,
    deps = [
        ":speed_rpm_serializer",
        "//score/serialization/someip:serializer",
    ],
)

cc_unit_test(
    name = "speed_rpm_serializer_test",
    srcs = ["speed_rpm_serializer_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":speed_rpm_serializer",
        "@score_baselibs//score/result",
    ],
)

# Integration test: loads the generated shared library with dlopen, resolves the factory symbols with
# dlsym and validates a serialize/deserialize round trip, mirroring how mw::com uses the library.
cc_test(
    name = "speed_rpm_serializer_integration_test",
    size = "small",
    timeout = "short",
    srcs = ["speed_rpm_serializer_integration_test.cpp"],
    data = [":libmwcom_serializer_someip.so"],
    features = COMPILER_WARNING_FEATURES,
    linkopts = ["-ldl"],
    deps = [
        ":speed_rpm",
        "//score/serialization/someip:serializer",
        "@bazel_tools//tools/cpp/runfiles",
        "@googletest//:gtest_main",
        "@score_baselibs//score/result",
    ],
)
