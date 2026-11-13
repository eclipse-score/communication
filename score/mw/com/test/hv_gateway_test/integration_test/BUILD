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
load("@rules_pkg//pkg:mappings.bzl", "pkg_filegroup")
load("//quality/integration_testing:integration_testing.bzl", "dual_qemu_integration_test")

exports_files(["dual_qemu_transport_config.json"])

pkg_filegroup(
    name = "consumer_filesystem",
    srcs = [
        "//score/mw/com/test/hv_gateway_test/consumer_app:consumer_app-pkg",
        "//score/mw/com/test/hv_gateway_test/gateway:gateway-pkg",
    ],
)

pkg_filegroup(
    name = "provider_filesystem",
    srcs = [
        "//score/mw/com/test/hv_gateway_test/gateway:gateway-pkg",
        "//score/mw/com/test/hv_gateway_test/provider_app:provider_app-pkg",
    ],
)

dual_qemu_integration_test(
    name = "test_hv_gateway",
    srcs = ["test_hv_gateway.py"],
    dual_config = "dual_qemu_transport_config.json",
    filesystem_a = "//score/mw/com/test/hv_gateway_test/integration_test:consumer_filesystem",
    filesystem_b = "//score/mw/com/test/hv_gateway_test/integration_test:provider_filesystem",
)
