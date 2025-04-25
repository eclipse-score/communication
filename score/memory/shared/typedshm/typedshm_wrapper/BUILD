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
load("//score/os:common_features.bzl", "COMPILER_WARNING_FEATURES")

selects.config_setting_group(
    name = "QNX_AND_USE_TYPED_SHMD",
    match_all = [
        "@platforms//os:qnx",
        "//score/memory/shared/flags:config_use_typedshmd",
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
        "//score/memory/shared/flags:config_use_typedshmd": [
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
        "//score/memory/shared:user_permission",
        "//score/os:errno_logging",
        "//score/os/utils/acl",
        "@score-baselibs//score/language/futurecpp",
    ] + select({
        ":QNX_AND_USE_TYPED_SHMD": [
            "//score/os/qnx:mman",
            # "@ddad//platform/aas/intc/typedmemd/code/clientlib:typedmemd_clientlib",
            # "@ddad//platform/aas/intc/typedmemd/code/clientlib:typedmemd_clientlib_interface",
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
        "//score/memory/shared/flags:config_use_typedshmd": ["@platforms//os:qnx"],
        "//conditions:default": ["@platforms//:incompatible"],
    }),
    visibility = [
        "//score/memory:__pkg__",
        # "@ddad//score/memory:__pkg__",
    ],
    deps = [
        "//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "//score/os/mocklib/qnx:mman_mock",
        "@googletest//:gtest_main",
        # "@ddad//platform/aas/intc/typedmemd/code/clientlib/mock:typedsharedmemory_mock",
    ],
)
