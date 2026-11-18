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
# error type and factory signatures) lives in //score/mw/com/impl/serialization; the datatype, its
# serializer and the factory below illustrate what the serializer generator emits. The serializer is
# built on the SomeIpStrategy, which encodes the datatype field by field.

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_shared_library", "cc_test")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

package(
    default_visibility = ["//score/mw/com:__subpackages__"],
)

cc_library(
    name = "speed_rpm",
    hdrs = ["speed_rpm.h"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
    ],
)

cc_library(
    name = "speed_rpm_serializer",
    srcs = ["speed_rpm_serializer.cpp"],
    hdrs = ["speed_rpm_serializer.h"],
    # The extern "C" factory symbols are not referenced by the library itself; keep them in the .so.
    alwayslink = True,
    deps = [
        ":speed_rpm",
        "//score/mw/com/impl/bindings/someip/serialization/strategy:someip_strategy",
        "//score/mw/com/impl/serialization:serializer",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/serialization/error",
    ],
)

# Generated SOME/IP serialization library loaded by mw::com via dlopen. The version script restricts the
# exported symbols to the factory functions resolved with dlsym (CreateSerializer_*, DestroySerializer).
cc_shared_library(
    name = "mwcom_serializer_someip",
    additional_linker_inputs = ["serializer_exports.lds"],
    shared_lib_name = "libmwcom_serializer_someip.so",
    user_link_flags = ["-Wl,--version-script=$(location :serializer_exports.lds)"],
    deps = [
        ":speed_rpm_serializer",
        "//score/mw/com/impl/serialization:serializer",
    ],
)

cc_unit_test(
    name = "speed_rpm_serializer_test",
    srcs = ["speed_rpm_serializer_test.cpp"],
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
    data = [":mwcom_serializer_someip"],
    linkopts = ["-ldl"],
    deps = [
        ":speed_rpm",
        "//score/mw/com/impl/serialization:serializer",
        "@bazel_tools//tools/cpp/runfiles",
        "@googletest//:gtest_main",
        "@score_baselibs//score/result",
    ],
)
