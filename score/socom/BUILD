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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/socom:feature_compatibility.bzl", "SOCOM_FEATURE_COMPATIBILITY")

filegroup(
    name = "headers",
    srcs = glob([
        "*.hpp",
    ]),
)

cc_library(
    name = "socom",
    srcs = glob([
        "impl/**/*.cpp",
        "impl/**/*.hpp",
    ]),
    hdrs = [":headers"],
    defines = [
        "WITH_SOCOM_DEADLOCK_DETECTION=1",
    ],
    features = COMPILER_WARNING_FEATURES,
    target_compatible_with = SOCOM_FEATURE_COMPATIBILITY,
    visibility = [
        "//:__pkg__",
        "//:__subpackages__",
        "@@score_someip_gateway+//:__pkg__",
        "@@score_someip_gateway+//:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/result",
    ],
)

filegroup(
    name = "mock_headers",
    srcs = glob(["mock/**"]),
)

cc_library(
    name = "mock",
    hdrs = [":mock_headers"],
    includes = ["mock"],
    target_compatible_with = SOCOM_FEATURE_COMPATIBILITY,
    visibility = [
        "//:__pkg__",
        "//:__subpackages__",
        "@@score_someip_gateway+//:__pkg__",
        "@@score_someip_gateway+//:__subpackages__",
    ],
    deps = [
        ":socom",
        "@googletest//:gtest_for_library",
    ],
)
