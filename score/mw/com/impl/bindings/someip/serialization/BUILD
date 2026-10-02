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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

package(
    default_visibility = ["//score/mw/com:__subpackages__"],
)

cc_library(
    name = "serialization_helpers",
    srcs = ["serialization_helpers.cpp"],
    hdrs = ["serialization_helpers.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com:__subpackages__"],
    deps = ["@score_baselibs//score/language/futurecpp"],
)

cc_test(
    name = "serialization_helpers_test",
    srcs = ["serialization_helpers_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":serialization_helpers",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp",
    ],
)
