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
    name = "com-api-concept",
    srcs = [
        "com_api_concept.rs",
        "reloc.rs",
    ],
    crate_root = "com_api_concept.rs",
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [],
)

rust_test(
    name = "com-api-concept-test",
    crate = ":com-api-concept",
    tags = [
        "manual",
        "no-clippy",
    ],
    visibility = ["//visibility:public"],
    deps = [":com-api-concept"],
)
