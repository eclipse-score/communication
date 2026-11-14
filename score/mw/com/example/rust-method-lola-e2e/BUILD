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
load("@rules_shell//shell:sh_test.bzl", "sh_test")

# Cross-process checks of the Rust method support on the LoLa runtime: producer and consumer
# processes, one scenario per failure mode (run_test.sh).
rust_binary(
    name = "rust_method_lola_e2e",
    srcs = ["main.rs"],
    features = ["link_std_cpp_lib"],
    deps = [
        "//score/mw/com/example/rust-method-lola-e2e/gen",
        "//score/mw/com/rust:score_com",
        "@score_communication_crate_index//:futures",
    ],
)

sh_test(
    name = "rust_method_lola_e2e_test",
    size = "large",
    srcs = ["run_test.sh"],
    args = [
        "$(rootpath :rust_method_lola_e2e)",
        "$(rootpath etc/mw_com_config.json.in)",
        "basic",
        "seq",
        "concurrent",
        "overflow",
        "queue",
        "two_consumers",
        "early",
        "crash",
        "handler_panic",
        "dup_appid",
        "dup_appid_producer",
    ],
    data = [
        "etc/mw_com_config.json.in",
        ":rust_method_lola_e2e",
    ],
    target_compatible_with = ["@platforms//os:linux"],
)
