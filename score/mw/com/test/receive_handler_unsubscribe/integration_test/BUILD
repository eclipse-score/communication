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

load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("//quality/integration_testing:integration_testing.bzl", "integration_test")

pkg_tar(
    name = "filesystem",
    deps = [
        "//score/mw/com/test/receive_handler_unsubscribe:receive_handler_unsubscribe-pkg",
    ],
)

integration_test(
    name = "receive_handler_unsubscribe",
    size = "small",
    srcs = ["test_receive_handler_unsubscribe.py"],
    filesystem = ":filesystem",
)
