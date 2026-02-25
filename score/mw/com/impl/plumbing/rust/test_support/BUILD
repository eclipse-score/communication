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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "test_helper_size_provider",
    srcs = ["test_helper_size_provider.cpp"],
    hdrs = ["test_helper_size_provider.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "//score/mw/com/impl/plumbing:sample_ptr",
    ],
)

rust_library(
    name = "test_utils_rs",
    srcs = ["test_utils.rs"],
    visibility = [
        "//score/mw/com/impl/plumbing/rust:__pkg__",
    ],
)

rust_library(
    name = "test_helper_size_ffi_rs",
    srcs = ["test_helper_size_ffi.rs"],
    features = ["link_std_cpp_lib"],
    visibility = [
        "//score/mw/com/impl/plumbing/rust:__pkg__",
    ],
    deps = [
        ":test_helper_size_provider",
        ":test_utils_rs",
    ],
)
