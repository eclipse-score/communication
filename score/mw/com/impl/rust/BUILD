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
load("@rules_rust//rust:defs.bzl", "rust_library")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

rust_library(
    name = "common",
    srcs = ["common.rs"],
)

cc_library(
    name = "proxy_bridge_cpp",
    srcs = ["proxy_bridge.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com:types",
        "//score/mw/com/impl:proxy_event_base",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "bridge_macros",
    srcs = ["bridge_macros.cpp"],
    hdrs = ["bridge_macros.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        "//score/mw/com:types",
        "//score/mw/com/impl",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_bridge_macros",
    hdrs = ["proxy_bridge_macros.h"],
    deprecation = "Please change your dependency to //score/mw/com/impl/rust:bridge_macros",
    visibility = [
        "//visibility:public", # platform_only
        # "@ddad//platform/aas:__subpackages__",
    ],
    deps = [":bridge_macros"],
)

rust_library(
    name = "proxy_bridge_rs",
    srcs = ["proxy_bridge.rs"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":proxy_bridge_cpp",
        "//score/mw/com/impl/plumbing:sample_ptr_rs",
        "@crate_index//:futures",
    ],
)

rust_library(
    name = "skeleton_bridge_rs",
    srcs = ["skeleton_bridge.rs"],
    visibility = [
        "//score/mw/com:__subpackages__",
        # "@ddad//score/mw/com:__subpackages__",
    ],
    deps = [
        ":common",
    ],
)

rust_library(
    name = "macros",
    srcs = ["macros.rs"],
    deps = [
        ":common",
        ":proxy_bridge_rs",
        ":skeleton_bridge_rs",
        "//score/mw/com/impl/plumbing:sample_ptr_rs",
    ],
)

rust_library(
    name = "mw_com",
    srcs = ["mw_com.rs"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":common",
        ":macros",
        ":proxy_bridge_rs",
        ":skeleton_bridge_rs",
    ],
)
