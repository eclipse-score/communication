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
    name = "sealedshm",
    srcs = [
        "sealed_shm.cpp",
    ],
    hdrs = [
        "i_sealed_shm.h",
        "sealed_shm.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/memory/shared:__subpackages__",
        "//score/memory/shared/test:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/os:errno_logging",
    ] + select({
        "@platforms//os:qnx": [
            "@score_baselibs//score/os/qnx:mman",
        ],
        "//conditions:default": [],
    }),
    alwayslink = True,
)

cc_library(
    name = "sealedshm_mock",
    testonly = True,
    srcs = [
        "sealed_shm_mock.cpp",
    ],
    hdrs = [
        "i_sealed_shm.h",
        "sealed_shm_mock.h",
    ],
    visibility = [
        "//score/memory/shared:__pkg__",
    ],
    deps = [
        "@googletest//:gtest",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/os:errno_logging",
    ],
)

cc_unit_test(
    name = "unit_test",
    srcs = [
        "sealed_shm_test.cpp",
    ],
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":sealedshm",
        ":sealedshm_mock",
        "@score_baselibs//score/os:stat",
    ] + select({
        "@platforms//os:linux": [],
        "@platforms//os:qnx": ["@score_baselibs//score/os/mocklib/qnx:mman_mock"],
    }),
)
