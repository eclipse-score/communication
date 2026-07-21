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
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

# Public C++ header library for code generation macros.
# This provides registration macros for generated C++ code that bridges Rust and C++ types.
cc_library(
    name = "register_interface",
    hdrs = ["register_interface.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public",  # For generated code and code generation infrastructure
    ],
    deps = [
        "//score/mw/com/impl/rust/com-api/com-api-ffi-lola:registry_bridge_macro_cpp",
    ],
)
