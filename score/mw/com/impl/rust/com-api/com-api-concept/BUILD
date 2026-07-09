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
load("//quality/unit_testing:unit_testing.bzl", "rust_unit_test")

rust_library(
    name = "com-api-concept",
    srcs = [
        "com_api_concept.rs",
        "error.rs",
        "interface_macros.rs",
        "lib.rs",
        "reloc.rs",
    ],
    edition = "2024",
    proc_macro_deps = [
        "//score/mw/com/impl/rust/com-api/com-api-concept-macros",
        "@score_communication_crate_index//:paste",
    ],
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        "@score_baselibs//src/containers",
        "@score_baselibs//src/log/score_log",
        "@score_communication_crate_index//:futures",
        "@score_communication_crate_index//:thiserror",
    ],
)

rust_test(
    name = "com-api-concept-test",
    crate = ":com-api-concept",
    edition = "2024",
    tags = ["manual"],
    deps = [":com-api-concept"],
)

rust_doc_test(
    name = "com-api-concept-macros-tests",
    crate = ":com-api-concept",
    # LIMITATION: rust_doc_test has a issue in rules_rust where it inherits crate_type="lib"
    # from the wrapped library instead of overriding to "bin" (like rust_test does).
    # This causes _add_native_link_flags() to skip CC native dependencies.
    # With LLVM's strict LLD linker, this results in undefined symbol errors when
    # the crate depends on C++ libraries. GCC masks this with broader archive scanning.
    tags = ["manual"],
    deps = ["//score/mw/com/impl/rust/com-api/com-api"],
)

rust_unit_test(
    name = "com-api-concept-macros-unit-tests",
    srcs = ["interface_macros.rs"],
    features = ["link_std_cpp_lib"],
    deps = ["//score/mw/com/impl/rust/com-api/com-api"],
)
