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
    features = ["link_std_cpp_lib"],
    deps = [
        "//score/mw/com/example/com-api-example/com-api-gen",
        "//score/mw/com/rust:score_com",
        "@score_baselibs//score/log_rust/score_log",
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
    env = {
        "MW_LOG_CONFIG_FILE": "score/mw/com/example/com-api-example/etc/logging.json",
    },
    features = ["link_std_cpp_lib"],
    deps = [
        ":com-api-example-lib",
        "//score/mw/com/example/com-api-example/com-api-gen",
        "//score/mw/com/rust:score_com",
        "@score_baselibs//score/log_rust/score_log",
        "@score_baselibs//score/log_rust/stdout_logger",
        "@score_communication_crate_index//:clap",
        "@score_communication_crate_index//:futures",
    ],
)

rust_test(
    name = "com-api-example-tokio-integration-test",
    srcs = ["tests_using_tokio_runtime.rs"],
    data = ["etc/mw_com_config.json"],
    features = ["link_std_cpp_lib"],
    # https://github.com/eclipse-score/communication/issues/794
    # Linux only, and disabled under sanitizers, until the findings in the Tokio multi-thread runtime are resolved.
    # The tests pass functionally, but sanitizer runs fail:
    #  - --config=asan_ubsan_lsan: LeakSanitizer reports leaks from tokio::runtime::scheduler::multi_thread::MultiThread::new
    #  - --config=tsan_ubsan: data race in <String as Clone>::clone between the test thread and a tokio-rt-worker thread
    # QNX: socket2 0.6.5 fails to build (unresolved import libc::TCP_KEEPIDLE),
    # see https://github.com/eclipse-score/communication/issues/1278
    target_compatible_with = ["@platforms//os:linux"] + select({
        "@score_cpp_policies//sanitizers/flags:any_sanitizer": ["@platforms//:incompatible"],
        "//conditions:default": [],
    }),
    deps = [
        "//score/mw/com/example/com-api-example/com-api-gen",
        "//score/mw/com/rust:score_com",
        "@score_communication_crate_index//:futures",
        "@score_communication_crate_index//:tokio",
    ],
)
