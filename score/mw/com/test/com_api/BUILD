# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "bigdata_com_api_gen_cpp",
    srcs = ["bigdata_com_api_gen.cpp"],
    hdrs = ["bigdata_com_api_gen.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl/rust/com-api/com-api-ffi-lola:registry_bridge_macro_cpp",
    ],
    deps = [
        "//score/mw/com/test/common_test_resources:bigdata_type",
    ],
    alwayslink = True,
)

rust_library(
    name = "bigdata_com_api_gen_rs",
    srcs = ["bigdata_com_api_gen.rs"],
    crate_name = "bigdata_com_api_gen",
    features = ["link_std_cpp_lib"],
    visibility = [
        "//score/mw/com/test/com_api:__subpackages__",
    ],
    deps = [
        ":bigdata_com_api_gen_cpp",
        "//score/mw/com/impl/rust/com-api/com-api",
    ],
)
