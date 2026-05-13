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

load("@rules_rust//rust:defs.bzl", "rust_binary")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

rust_binary(
    name = "bigdata-consumer",
    srcs = ["consumer_app.rs"],
    features = ["link_std_cpp_lib"],
    # linux_x86_64_score_gcc_12_2_0_posix toolchain is built without --enable-default-pie, so its static C++ libs are non-PIC.
    # When Bazel builds this binary in exec config (py_test build with rule cfg=exec), the Rust linker
    # defaults to PIE and rejects non-PIC archives. -no-pie disables PIE; -lstdc++ is needed because
    # the link_std_cpp_lib CC feature is not activated in exec config.
    rustc_flags = select({
        "@platforms//os:linux": [
            "-Clink-arg=-no-pie",
            "-Clink-arg=-lstdc++",
        ],
        "//conditions:default": [],
    }),
    visibility = [
        "//score/mw/com/test/basic_rust_api:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl/rust/com-api/com-api",
        "//score/mw/com/test/basic_rust_api:bigdata_com_api_gen_rs",
        "@score_communication_crate_index//:clap",
    ],
)

pkg_application(
    name = "bigdata-com-api-sync-pkg",
    app_name = "bigdata-com-api-sync",
    bin = [
        ":bigdata-consumer",
        "//score/mw/com/test/basic_rust_api/producer_app:bigdata-producer",
    ],
    etc = [
        "//score/mw/com/test/basic_rust_api/etc:config",
        "//score/mw/com/test/bigdata:logging.json",
    ],
    visibility = [
        "//score/mw/com/test/basic_rust_api:__subpackages__",
    ],
)
