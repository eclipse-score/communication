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

load("@rules_rust//rust:defs.bzl", "rust_test")

rust_test(
    name = "proxy_bridge_integration_test",
    srcs = ["proxy_skeleton_bridge_int_test.rs"],
    data = ["//score/mw/com/example/ipc_bridge:etc/mw_com_config.json"],
    tags = [
        "manual",
        "no-clippy",
    ],
    visibility = ["//:__subpackages__"],
    deps = [
        "//score/mw/com/example/ipc_bridge:ipc_bridge_gen_rs",
        "//score/mw/com/impl/rust:proxy_bridge_rs",
    ],
)
