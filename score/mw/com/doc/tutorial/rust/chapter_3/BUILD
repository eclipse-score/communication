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

rust_binary(
    name = "producer_app",
    srcs = [
        "main.rs",
        "producer.rs",
    ],
    crate_root = "main.rs",
    data = ["//score/mw/com/doc/tutorial/rust/common:config"],
    env = {
        "MW_LOG_CONFIG_FILE": "score/mw/com/doc/tutorial/rust/common/etc/logging.json",
    },
    features = ["link_std_cpp_lib"],
    deps = [
        "//score/mw/com/doc/tutorial/rust/common",
        "//score/mw/com/doc/tutorial/rust/common/com-api-gen",
        "//score/mw/com/rust:score_com",
        "@score_baselibs//score/log_rust/score_log",
        "@score_communication_crate_index//:clap",
    ],
)

filegroup(
    name = "doc_files",
    srcs = glob([
        "*.rst",
        "*.rs",
    ]),
    visibility = ["//score/mw/com/doc/tutorial/rust:__pkg__"],
)
