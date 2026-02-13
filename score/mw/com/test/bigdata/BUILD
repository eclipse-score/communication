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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_binary(
    name = "bigdata",
    srcs = [
        "big_datatype_application.cpp",
        "big_datatype_application.h",
    ],
    data = ["mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:sample_sender_receiver",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
    ],
)

cc_library(
    name = "bigdata_gen_cpp",
    srcs = ["bigdata_gen.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/impl/rust:bridge_macros",
        "//score/mw/com/test/common_test_resources:bigdata_type",
    ],
)

rust_library(
    name = "bigdata_gen_rs",
    srcs = ["bigdata_gen.rs"],
    deps = [
        ":bigdata_gen_cpp",
        "//score/mw/com/impl/rust:mw_com",
        "@crate_index//:libc",
    ],
)

rust_binary(
    name = "bigdata_rs",
    srcs = ["bigdata.rs"],
    data = ["mw_com_config.json"],
    features = ["link_std_cpp_lib"],
    deps = [
        ":bigdata_gen_rs",
        "//score/mw/com/impl/rust:mw_com",
        "@crate_index//:futures",
    ],
)

rust_binary(
    name = "bigdata_skeleton_rs",
    srcs = ["bigdata_skeleton.rs"],
    data = ["mw_com_config.json"],
    features = ["link_std_cpp_lib"],
    deps = [
        ":bigdata_gen_rs",
        "//score/mw/com/impl/rust:proxy_bridge_rs",
    ],
)

pkg_application(
    name = "bigdata-pkg",
    app_name = "bigdata",
    bin = [":bigdata"],
    etc = [
        "logging.json",
        "mw_com_config.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//pqr/abc/abc-lmn/config/iproject/coreos/hardware/hypervisor:__subpackages__",
        "//score/mw/com/test/bigdata:__subpackages__",
    ],
)
