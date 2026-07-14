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

load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")

cc_binary(
    name = "app2",
    srcs = ["app2_main.cpp"],
    deps = [
        "//score/mw/com/gateway/transport_layer/sample:bidirectional_transport",
    ],
)

cc_binary(
    name = "app1",
    srcs = ["app1_main.cpp"],
    deps = [
        "//score/mw/com/gateway/transport_layer/sample:bidirectional_transport",
    ],
)

pkg_files(
    name = "application-pkg",
    srcs = [
        ":app1",
        ":app2",
    ],
    attributes = pkg_attributes(
        mode = "0777",
    ),
    prefix = "/",
    visibility = ["//score/mw/com/gateway/transport_layer/sample/test:__subpackages__"],
)
