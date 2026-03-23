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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

selects.config_setting_group(
    name = "QNX_AND_USE_TYPED_SHMD",
    match_all = [
        "@platforms//os:qnx",
        "@score_baselibs//score/memory/shared/flags:config_use_typedshmd",
    ],
)

cc_library(
    name = "typed_memory_utils",
    srcs = ["typed_memory_utils.cpp"],
    hdrs = ["typed_memory_utils.h"],
    defines = select({
        "@score_baselibs//score/memory/shared/flags:config_use_typedshmd": [
            "USE_TYPEDSHMD",
            "ALLOW_QM",
        ],
        "//conditions:default": ["ALLOW_QM"],
    }),
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "@score_baselibs//score/memory/shared:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/mw/log:frontend",
        "@score_baselibs//score/os:errno_logging",
        "@score_baselibs//score/os:stat",
        "@score_baselibs//score/os:unistd",
    ],
)
