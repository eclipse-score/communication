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
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "bounds_check_memory_pool",
    testonly = True,
    srcs = ["bounds_check_memory_pool.cpp"],
    hdrs = ["bounds_check_memory_pool.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory/shared/test:__subpackages__",
    ],
    deps = [
        ":offset_ptr_test_resources",
        "//score/memory/shared",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "offset_ptr_test_resources",
    testonly = True,
    srcs = ["offset_ptr_test_resources.cpp"],
    hdrs = ["offset_ptr_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/memory/shared/test:__subpackages__"],
    deps = [
        "//score/memory/shared",
        "//score/memory/shared/fake:fake_memory_resources",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp",
    ],
)
