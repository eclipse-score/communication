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

# Default target with lola runtime
rust_library(
    name = "com-api",
    srcs = ["com_api.rs"],
    crate_name = "com_api",
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "//score/mw/com/impl/rust/com-api/com-api-concept",
        "//score/mw/com/impl/rust/com-api/com-api-runtime-lola",
        "//score/mw/com/impl/rust/com-api/com-api-runtime-mock",
    ],
)
