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

# Default target with lola runtime
rust_library(
    name = "score_com",
    srcs = ["score_com.rs"],
    crate_name = "score_com",
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "//score/mw/com/impl/rust/com-api/com-api-runtime-lola",
        "//score/mw/com/rust/score_com_concept",
    ],
)

# Test-only target that additionally exposes the mock runtime.
# Use this target (instead of :score_com) in test BUILD rules that need MockRuntimeBuilderImpl.
# Mock Runtime is not yet ready - https://github.com/eclipse-score/communication/issues/490
rust_library(
    name = "score_com_mock",
    testonly = True,
    srcs = ["score_com_mock.rs"],
    crate_name = "score_com",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":score_com",
        "//score/mw/com/impl/rust/com-api/com-api-runtime-mock",
    ],
)
