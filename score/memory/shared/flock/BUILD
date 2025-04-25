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
load("//bazel:unit_tests.bzl", "cc_gtest_unit_test")
load("//score/os:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "flock_mutex",
    srcs = ["flock_mutex.cpp"],
    hdrs = ["flock_mutex.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/os:fcntl",
        "//score/mw/log",
    ],
    tags = ["FFI"],
    deps = [
        "//score/memory/shared:lock_file",
    ],
)

cc_library(
    name = "exclusive_flock_mutex",
    srcs = ["exclusive_flock_mutex.cpp"],
    hdrs = ["exclusive_flock_mutex.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/os:fcntl",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
        # "@ddad//score/memory:__subpackages__",
    ],
    deps = [
        ":flock_mutex",
        "//score/memory/shared:lock_file",
    ],
)

cc_library(
    name = "shared_flock_mutex",
    srcs = ["shared_flock_mutex.cpp"],
    hdrs = ["shared_flock_mutex.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/os:fcntl",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
        # "@ddad//score/memory:__subpackages__",
    ],
    deps = [
        ":flock_mutex",
        "//score/memory/shared:lock_file",
    ],
)

cc_library(
    name = "flock_mutex_and_lock",
    srcs = ["flock_mutex_and_lock.cpp"],
    hdrs = ["flock_mutex_and_lock.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
        # "@ddad//score/memory:__subpackages__",
    ],
    deps = [
        "//score/memory/shared:lock_file",
    ],
)

cc_gtest_unit_test(
    name = "unit_test",
    srcs = [
        "flock_mutex_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
        # "@ddad//score/memory:__pkg__",
    ],
    deps = [
        ":exclusive_flock_mutex",
        ":shared_flock_mutex",
        "//score/os:fcntl",
        "//score/os/mocklib:fcntl_mock",
        "//score/os/mocklib:stat_mock",
    ],
)
