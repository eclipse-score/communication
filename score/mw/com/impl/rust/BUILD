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
    name = "proxy_bridge_macros",
    srcs = ["proxy_bridge_macros.cpp"],
    hdrs = ["proxy_bridge_macros.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public", # platform_only
        # "@ddad//platform/aas:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "@score-baselibs//score/language/futurecpp",
    ],
)

rust_library(
    name = "proxy_bridge_rs",
    srcs = ["proxy_bridge.rs"],
    visibility = [
        "//score/mw/com:__subpackages__",
        # "@ddad//score/mw/com:__subpackages__",
    ],
    deps = [
        ":proxy_bridge_cpp",
        "//score/mw/com/impl/plumbing:sample_ptr_rs",
        "@crate_index//:futures",
    ],
)

rust_library(
    name = "macros",
    srcs = ["macros.rs"],
    deps = [
        ":proxy_bridge_rs",
        "//score/mw/com/impl/plumbing:sample_ptr_rs",
    ],
)

rust_library(
    name = "mw_com",
    srcs = ["mw_com.rs"],
    visibility = [
        "//visibility:public", # platform_only
        # "@ddad//platform/aas:__subpackages__",
    ],
    deps = [
        ":macros",
        ":proxy_bridge_rs",
    ],
)
