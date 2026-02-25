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

load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("//quality/integration_testing:integration_testing.bzl", "integration_test")

cc_binary(
    name = "example-app",
    srcs = ["main.cpp"],
)

cc_binary(
    name = "example-app2",
    srcs = ["main2.cpp"],
)

pkg_tar(
    name = "example-app-pkg",
    srcs = [
        ":example-app",
        ":example-app2",
    ],
)

integration_test(
    name = "test_hello_world",
    srcs = [
        "test_hello_world.py",
    ],
    filesystem = ":example-app-pkg",
)
