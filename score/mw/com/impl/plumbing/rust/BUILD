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

load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

rust_library(
    name = "common_rs",
    srcs = ["common.rs"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "@score_communication_crate_index//:libc",
    ],
)

rust_library(
    name = "sample_allocatee_ptr_rs",
    srcs = ["sample_allocatee_ptr.rs"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [":common_rs"],
)

rust_library(
    name = "sample_ptr_rs",
    srcs = ["sample_ptr.rs"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":common_rs",
        "@score_communication_crate_index//:libc",
    ],
)

rust_test(
    name = "sample_allocatee_ptr_test_rs",
    srcs = ["sample_allocatee_ptr.rs"],
    features = ["link_std_cpp_lib"],
    # Disabled for thread sanitizer because of detected race (Ticket-246891)
    target_compatible_with = ["//quality/sanitizer/constraints:no_tsan"],
    deps = [
        ":common_rs",
        "//score/mw/com/impl/plumbing/rust/test_support:test_helper_size_ffi_rs",
        "//score/mw/com/impl/plumbing/rust/test_support:test_utils_rs",
    ],
)

rust_test(
    name = "sample_ptr_test_rs",
    srcs = ["sample_ptr.rs"],
    features = ["link_std_cpp_lib"],
    # Disabled for thread sanitizer because of detected race (Ticket-246891)
    target_compatible_with = ["//quality/sanitizer/constraints:no_tsan"],
    deps = [
        ":common_rs",
        "//score/mw/com/impl/plumbing/rust/test_support:test_helper_size_ffi_rs",
        "//score/mw/com/impl/plumbing/rust/test_support:test_utils_rs",
        "@score_communication_crate_index//:libc",
    ],
)
