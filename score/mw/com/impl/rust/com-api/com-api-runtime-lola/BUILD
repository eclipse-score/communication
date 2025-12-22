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

rust_library(
    name = "com-api-runtime-lola",
    srcs = ["runtime.rs"],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "//score/mw/com/impl/plumbing:sample_ptr_rs",
        "//score/mw/com/impl/rust:generic_bridge_ffi_rs",
        "//score/mw/com/impl/rust:proxy_bridge_rs",
        "//score/mw/com/impl/rust/com-api/com-api-concept",
        "@crate_index//:futures",
    ],
)
