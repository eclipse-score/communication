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
load("@bazel_skylib//lib:selects.bzl", "selects")
load("@score-baselibs//bazel:unit_tests.bzl", "cc_unit_test_suites_for_host_and_qnx")
load("@score-baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

selects.config_setting_group(
    name = "QNX_AND_USE_TYPED_SHMD",
    match_all = [
        "@platforms//os:qnx",
        "@score-baselibs//score/memory/shared/flags:config_use_typedshmd",
    ],
)

cc_library(
    name = "typedmemory",
    srcs = [
        "typed_memory.cpp",
    ],
    hdrs = [
        "typed_memory.h",
    ],
    defines = select({
        "@score-baselibs//score/memory/shared/flags:config_use_typedshmd": [
            "USE_TYPEDSHMD",
            "ALLOW_QM",
        ],
        "//conditions:default": ["ALLOW_QM"],
    }),
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@score-baselibs//score/memory/shared:user_permission",
        "@score-baselibs//score/os:errno_logging",
        "@score-baselibs//score/os/utils/acl",
        "@score-baselibs//score/language/futurecpp",
    ] + select({
        ":QNX_AND_USE_TYPED_SHMD": [
            "//platform/aas/intc/typedmemd/code/clientlib:typedmemd_clientlib",
            "//platform/aas/intc/typedmemd/code/clientlib:typedmemd_clientlib_interface",
            "@score-baselibs//score/os/qnx:mman",
        ],
        "//conditions:default": [],
    }),
    alwayslink = True,
)

cc_test(
    name = "typed_memory_test",
    srcs = ["typed_memory_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    tags = [
        "manual",  # to disable using as host unit test in CI
        "unit",
    ],
    target_compatible_with = select({
        "@score-baselibs//score/memory/shared/flags:config_use_typedshmd": ["@platforms//os:qnx"],
        "//conditions:default": ["@platforms//:incompatible"],
    }),
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        "//platform/aas/intc/typedmemd/code/clientlib/mock:typedsharedmemory_mock",
        "@score-baselibs//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "@score-baselibs//score/os/mocklib/qnx:mman_mock",
        "@googletest//:gtest_main",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":typed_memory_test",
    ],
    visibility = [
        "@score-baselibs//score/memory/shared:__pkg__",
    ],
)
