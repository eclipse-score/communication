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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

rust_library(
    name = "com-api-gen",
    srcs = ["com_api_gen.rs"],
    crate_name = "com_api_gen",
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl/rust/com-api/com-api",
    ],
)

cc_library(
    name = "vehicle_gen_cpp",
    srcs = ["vehicle_gen.cpp"],
    hdrs = ["vehicle_gen.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl/rust:registry_bridge_macro_cpp",
    ],
)
