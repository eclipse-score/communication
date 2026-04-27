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

load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_test")

rust_binary(
    name = "com-api-example",
    srcs = ["basic-consumer-producer.rs"],
    data = ["etc/mw_com_config.json"],
    features = ["link_std_cpp_lib"],
    # The score GCC toolchains (e.g. linux_aarch64_score_gcc_12_2_0_posix) are built without
    # --enable-default-pie, so their static C++ and system libs (e.g. libacl) are non-PIC.
    # The Rust linker defaults to PIE and rejects non-PIC objects.
    # -no-pie disables PIE, -lstdc++ is needed when link_std_cpp_lib CC feature is not activated
    # (e.g. exec config builds). -lgcc is needed on aarch64: GCC emits outline atomic helper
    # calls (__aarch64_ldadd4_acq_rel etc.) and these live in the toolchain's libgcc.a, not in
    # the sysroot's libgcc_s.so linked by Rust by default.
    rustc_flags = select({
        "@platforms//cpu:aarch64": [
            "-Clink-arg=-no-pie",
            "-Clink-arg=-lgcc",
        ],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = [
        "//score/mw/com/example/com-api-example/com-api-gen",
        "//score/mw/com/impl/rust/com-api/com-api",
        "@score_communication_crate_index//:clap",
    ],
)

rust_test(
    name = "com-api-example-test",
    crate = ":com-api-example",
    data = ["etc/mw_com_config.json"],
    rustc_flags = select({
        "@platforms//cpu:aarch64": [
            "-Clink-arg=-no-pie",
            "-Clink-arg=-lgcc",
        ],
        "//conditions:default": [],
    }),
    tags = ["manual"],
    deps = [
        ":com-api-example",
        "@score_communication_crate_index//:futures",
        "@score_communication_crate_index//:tokio",
    ],
)
