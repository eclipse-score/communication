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

load("@rules_rust//rust:defs.bzl", "rust_test")

rust_test(
    name = "tokio-usage",
    srcs = ["tokio_usage.rs"],
    data = ["//score/mw/com/doc/tutorial/rust/common:config"],
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
        "//score/mw/com/doc/tutorial/rust/common/com-api-gen",
        "//score/mw/com/rust:score_com",
        # [rust-tokio-deps-start]
        "@score_communication_crate_index//:futures",
        "@score_communication_crate_index//:tokio",
        # [rust-tokio-deps-end]
    ],
)

filegroup(
    name = "doc_files",
    srcs = glob([
        "*.rst",
        "*.rs",
    ]) + ["BUILD"],
    visibility = ["//score/mw/com/doc/tutorial/rust:__pkg__"],
)
