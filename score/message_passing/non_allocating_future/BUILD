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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "non_allocating_future",
    srcs = [
        "non_allocating_future.cpp",
    ],
    hdrs = [
        "non_allocating_future.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/message_passing:__subpackages__"],
)

cc_unit_test(
    name = "non_allocating_future_test",
    srcs = [
        "non_allocating_future_samples_test.cpp",
        "non_allocating_future_unit_test.cpp",
    ],
    visibility = ["//score/message_passing:__subpackages__"],
    deps = [
        ":non_allocating_future",
    ],
)
