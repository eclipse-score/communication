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

load("@rules_rust//rust:defs.bzl", "rust_library")

# Compile-only demonstration of the method API.
rust_library(
    name = "methods",
    srcs = glob(["*.rs"]),
    crate_name = "tutorial_methods",
    crate_root = "lib.rs",
    features = ["link_std_cpp_lib"],
    deps = [
        "//score/mw/com/doc/tutorial/rust/common/com-api-gen",
        "//score/mw/com/rust:score_com",
    ],
)

filegroup(
    name = "doc_files",
    srcs = glob([
        "*.rst",
        "*.rs",
    ]),
    visibility = ["//score/mw/com/doc/tutorial/rust:__pkg__"],
)
