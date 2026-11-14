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

rust_library(
    name = "gen",
    srcs = ["calc_gen.rs"],
    crate_name = "rust_method_lola_e2e_gen",
    features = ["link_std_cpp_lib"],
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        ":calc_gen_cpp",
        "//score/mw/com/rust:score_com",
    ],
)

cc_library(
    name = "calc_gen_cpp",
    srcs = ["calc_gen.cpp"],
    hdrs = ["calc_gen.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/rust/score_com_cpp_bridge:register_interface",
    ],
    alwayslink = True,
)
