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
load("@rules_rust//rust:defs.bzl", "rust_doc_test", "rust_proc_macro")

rust_proc_macro(
    name = "com-api-concept-macros",
    srcs = ["lib.rs"],
    crate_name = "com_api_concept_macros",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@crate_index//:quote",
        "@crate_index//:syn",
    ],
)

rust_doc_test(
    name = "com-api-concept-macros-tests",
    crate = ":com-api-concept-macros",
)
