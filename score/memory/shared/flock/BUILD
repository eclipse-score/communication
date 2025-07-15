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
load("@score-baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("@score-baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "flock_mutex",
    srcs = ["flock_mutex.cpp"],
    hdrs = ["flock_mutex.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/os:fcntl",
        "@score-baselibs//score/mw/log",
    ],
    tags = ["FFI"],
    deps = [
        "@score-baselibs//score/memory/shared:lock_file",
    ],
)

cc_library(
    name = "exclusive_flock_mutex",
    srcs = ["exclusive_flock_mutex.cpp"],
    hdrs = ["exclusive_flock_mutex.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/os:fcntl",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":flock_mutex",
        "@score-baselibs//score/memory/shared:lock_file",
    ],
)

cc_library(
    name = "shared_flock_mutex",
    srcs = ["shared_flock_mutex.cpp"],
    hdrs = ["shared_flock_mutex.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/os:fcntl",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":flock_mutex",
        "@score-baselibs//score/memory/shared:lock_file",
    ],
)

cc_library(
    name = "flock_mutex_and_lock",
    srcs = ["flock_mutex_and_lock.cpp"],
    hdrs = ["flock_mutex_and_lock.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        "@score-baselibs//score/memory/shared:lock_file",
    ],
)

cc_gtest_unit_test(
    name = "unit_test",
    srcs = [
        "flock_mutex_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":exclusive_flock_mutex",
        ":shared_flock_mutex",
        "@score-baselibs//score/os:fcntl",
        "@score-baselibs//score/os/mocklib:fcntl_mock",
        "@score-baselibs//score/os/mocklib:stat_mock",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":unit_test",
    ],
    visibility = [
        "@score-baselibs//score/memory/shared:__pkg__",
    ],
)
