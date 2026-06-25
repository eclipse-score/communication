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
load("@rules_rust//rust:defs.bzl", "rust_library")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "registry_bridge_macro_cpp",
    srcs = ["registry_bridge_macro.cpp"],
    hdrs = ["registry_bridge_macro.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "//score/mw/com:runtime",
        "//score/mw/com:runtime_configuration",
        "//score/mw/com:types",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "@score_baselibs//score/language/futurecpp",
    ],
)

rust_library(
    name = "bridge_ffi_rs",
    srcs = ["bridge_ffi.rs"],
    edition = "2024",
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":registry_bridge_macro_cpp",
        "//score/mw/com/impl/plumbing/rust:sample_ptr_rs",
    ],
)

rust_library(
    name = "bridge_ffi_mock",
    srcs = ["bridge_ffi_mock.rs"],
    edition = "2024",
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":bridge_ffi_rs",
        "//score/mw/com/impl/plumbing/rust:sample_allocatee_ptr_rs",
        "@score_communication_crate_index//:mockall",
    ],
)

rust_library(
    name = "bridge_ffi_lola",
    srcs = ["bridge_ffi_lola.rs"],
    edition = "2024",
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":bridge_ffi_rs",
    ],
)
