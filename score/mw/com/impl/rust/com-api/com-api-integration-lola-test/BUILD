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
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

filegroup(
    name = "integration_test_config",
    srcs = ["config.json"],
    visibility = ["//:__subpackages__"],
)

cc_library(
    name = "integration_test_gen_cpp",
    srcs = ["integration_test_gen.cpp"],
    hdrs = ["integration_test_gen.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl/rust/com-api/com-api-ffi-lola:registry_bridge_macro_cpp",
    ],
    alwayslink = True,
)

rust_library(
    name = "integration-test-lola",
    srcs = [
        "integration_test.rs",
        "lib.rs",
        "test_types.rs",
    ],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        ":integration_test_gen_cpp",
        "//score/mw/com/impl/rust/com-api/com-api",
        "//score/mw/com/impl/rust/com-api/com-api-runtime-lola",
    ],
)

rust_test(
    name = "integration-test",
    crate = ":integration-test-lola",
    data = [":integration_test_config"],
    features = ["link_std_cpp_lib"],
    deps = [
        ":integration-test-lola",
        ":integration_test_gen_cpp",
        "//score/mw/com/impl/rust/com-api/com-api",
        "//score/mw/com/impl/rust/com-api/com-api-runtime-lola",
    ],
)
