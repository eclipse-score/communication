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
load("//quality/integration_testing:integration_testing.bzl", "dual_qemu_integration_test")

exports_files(["dual_qemu_transport_config.json"])

dual_qemu_integration_test(
    name = "test_qemu_transport",
    srcs = ["test_qemu_transport.py"],
    dual_config = "dual_qemu_transport_config.json",
    filesystem_a = "//score/mw/com/gateway/transport_layer/qemu/test:qemu_transport_test_apps",
    filesystem_b = "//score/mw/com/gateway/transport_layer/qemu/test:qemu_transport_test_apps",
)
