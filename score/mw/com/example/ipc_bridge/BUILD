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
load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_library")
load("@safe_posix_platform@score-baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_binary(
    name = "ipc_bridge_cpp",
    srcs = [
        "assert_handler.cpp",
        "assert_handler.h",
        "main.cpp",
    ],
    data = ["etc/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":sample_sender_receiver",
        "//score/mw/com",
        "@score-baselibs//score/mw/log",
        "//third_party/boost:program_options",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "sample_sender_receiver",
    srcs = [
        "sample_sender_receiver.cpp",
    ],
    hdrs = [
        "sample_sender_receiver.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":datatype",
        "//score/mw/com",
        "@score-baselibs//score/mw/log",
    ],
)

cc_library(
    name = "datatype",
    srcs = [
        "datatype.cpp",
    ],
    hdrs = [
        "datatype.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
        "@score-baselibs//score/language/futurecpp",
    ],
)

rust_library(
    name = "lib_gen_rs",
    srcs = ["lib_gen.rs"],
    deps = [
        ":lib_gen_cpp",
        "//score/mw/com/impl/plumbing:sample_ptr_rs",
        "//score/mw/com/impl/rust:proxy_bridge_rs",
        "@crate_index//:libc",
    ],
)

rust_binary(
    name = "ipc_bridge_rs",
    srcs = ["ipc_bridge.rs"],
    data = ["etc/mw_com_config.json"],
    features = ["link_std_cpp_lib"],
    deps = [
        ":lib_gen_rs",
        "//score/mw/com/impl/rust:proxy_bridge_rs",
        "@crate_index//:futures",
    ],
)

cc_library(
    name = "lib_gen_cpp",
    srcs = ["lib_gen.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":datatype",
    ],
)
