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
    name = "com-api-runtime-lola",
    srcs = [
        "consumer.rs",
        "lib.rs",
        "producer.rs",
        "runtime.rs",
    ],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "//score/mw/com/impl/plumbing/rust:sample_allocatee_ptr_rs",
        "//score/mw/com/impl/plumbing/rust:sample_ptr_rs",
        "//score/mw/com/impl/rust:mw_com",
        "//score/mw/com/impl/rust/com-api/com-api-concept",
        "//score/mw/com/impl/rust/com-api/com-api-ffi-lola:bridge_ffi_rs",
        "@score_communication_crate_index//:futures",
    ],
)

rust_test(
    name = "com-api-runtime-lola-tests",
    crate = ":com-api-runtime-lola",
    tags = ["manual"],
    deps = [":com-api-runtime-lola"],
)

rust_doc_test(
    name = "com-api-runtime-lola-doc-tests",
    crate = ":com-api-runtime-lola",
    target_compatible_with = ["@platforms//os:linux"],
    deps = [":com-api-runtime-lola"],
)
