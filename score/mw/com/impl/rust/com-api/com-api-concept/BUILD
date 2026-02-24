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
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test","rust_doc_test")

rust_library(
    name = "com-api-concept",
    srcs = [
        "lib.rs",
        "com_api_concept.rs",
        "reloc.rs",
        "interface_macros.rs",
    ],
    proc_macro_deps = [
        "//score/mw/com/impl/rust/com-api/com-api-concept-macros",
        "@crate_index//:paste",
    ],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "@score_baselibs_rust//src/containers",
    ],
)

rust_test(
    name = "com-api-concept-test",
    crate = ":com-api-concept",
    tags = ["manual"],
    deps = [":com-api-concept"],
)

rust_doc_test(
    name = "com-api-concept-macros-tests",
    crate = ":com-api-concept",
    deps = ["//score/mw/com/impl/rust/com-api/com-api",],
)
