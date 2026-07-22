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

load("@rules_rust//rust:defs.bzl", "rust_doc_test", "rust_proc_macro")

rust_proc_macro(
    name = "com-api-macros",
    srcs = ["lib.rs"],
    crate_name = "com_api_concept_macros",
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "@score_communication_crate_index//:quote",
        "@score_communication_crate_index//:syn",
    ],
)

rust_doc_test(
    name = "com-api-macros-tests",
    crate = ":com-api-macros",
    target_compatible_with = ["@platforms//os:linux"],
)
