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
load("//score/socom:feature_compatibility.bzl", "SOCOM_FEATURE_COMPATIBILITY")

cc_library(
    name = "socom_test_framework",
    srcs = glob(include = ["src/**"]),
    hdrs = glob(["inc/**"]),
    includes = [
        "inc",
        "score/socom/test/framework/inc",
    ],
    target_compatible_with = SOCOM_FEATURE_COMPATIBILITY,
    visibility = [
        "//score/socom:__subpackages__",
    ],
    deps = [
        "//score/socom",
        "//score/socom:mock",
        "@googletest//:gtest_for_library",
    ],
)
