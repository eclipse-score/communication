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

load("@rules_rust//rust:defs.bzl", "rust_doc_test", "rust_library", "rust_test")

rust_library(
    name = "com-api-runtime-mock",
    srcs = ["runtime.rs"],
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        "//score/mw/com/rust/score_com_concept",
        "@score_communication_crate_index//:futures",
    ],
)

rust_test(
    name = "com-api-runtime-mock-tests",
    crate = ":com-api-runtime-mock",
    # Linux only until QNX build is enabled for Rust tests: https://github.com/eclipse-score/communication/issues/1278
    target_compatible_with = ["@platforms//os:linux"],
    deps = [":com-api-runtime-mock"],
)

rust_doc_test(
    name = "com-api-runtime-mock-doc-tests",
    crate = ":com-api-runtime-mock",
    target_compatible_with = ["@platforms//os:linux"],
    deps = [":com-api-runtime-mock"],
)

rust_test(
    name = "generated-interface-test",
    srcs = ["generated_interface_test.rs"],
    target_compatible_with = ["@platforms//os:linux"],
    deps = ["//score/mw/com/rust:score_com_mock"],
)
