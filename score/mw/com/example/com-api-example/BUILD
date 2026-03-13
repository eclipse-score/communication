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
    data = [
        "etc/config.json",
        "etc/logging.json",
    ],
    env = {
        "MW_LOG_CONFIG_FILE": "score/mw/com/example/com-api-example/etc/logging.json",
    },
    features = ["link_std_cpp_lib"],
    visibility = ["//visibility:public"],
    deps = [
        "//score/mw/com/example/com-api-example/com-api-gen",
        "//score/mw/com/impl/rust/com-api/com-api",
        "@score_baselibs_rust//src/log/score_log",
        "@score_logging//score/mw/log/rust/score_log_bridge",
    ],
)

rust_test(
    name = "com-api-example-test",
    crate = ":com-api-example",
    data = [
        "etc/config.json",
        "etc/logging.json",
    ],
    env = {
        "MW_LOG_CONFIG_FILE": "score/mw/com/example/com-api-example/etc/logging.json",
    },
    tags = ["manual"],
    deps = [
        ":com-api-example",
        "@score_communication_crate_index//:futures",
        "@score_communication_crate_index//:tokio",
    ],
)
