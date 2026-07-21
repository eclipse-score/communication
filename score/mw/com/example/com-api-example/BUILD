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

load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_library", "rust_test")

rust_library(
    name = "com-api-example-lib",
    srcs = glob(["src/**/*.rs"]),
    crate_name = "com_api_example",
    edition = "2024",
    features = ["link_std_cpp_lib"],
    deps = [
        "//score/mw/com/example/com-api-example/com-api-gen",
        "@score_baselibs//src/log/score_log",
        "//score/mw/com/rust:api",
        "@score_communication_crate_index//:clap",
        "@score_communication_crate_index//:futures",
    ],
)

rust_binary(
    name = "com-api-example",
    srcs = ["main.rs"],
    data = [
        "etc/logging.json",
        "etc/mw_com_config.json",
    ],
    edition = "2024",
    env = {
        "MW_LOG_CONFIG_FILE": "score/mw/com/example/com-api-example/etc/logging.json",
    },
    features = ["link_std_cpp_lib"],
    deps = [
        ":com-api-example-lib",
        "//score/mw/com/example/com-api-example/com-api-gen",
        "//score/mw/com/impl/rust/com-api/com-api",
        "@score_baselibs//src/log/score_log",
        "@score_baselibs//src/log/stdout_logger",
        "@score_communication_crate_index//:clap",
        "@score_communication_crate_index//:futures",
    ],
)

rust_test(
    name = "com-api-example-tokio-integration-test",
    srcs = ["tests_using_tokio_runtime.rs"],
    data = ["etc/mw_com_config.json"],
    edition = "2024",
    features = ["link_std_cpp_lib"],
    tags = ["manual"],
    deps = [
        "//score/mw/com/example/com-api-example/com-api-gen",
        "//score/mw/com/impl/rust/com-api/com-api",
        "@score_communication_crate_index//:futures",
        "@score_communication_crate_index//:tokio",
    ],
)
