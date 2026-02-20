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
        "//score/mw/com/test/partial_restart/consumer_restart:consumer_restart-pkg",
    ],
)

integration_test(
    name = "consumer_restart",
    timeout = "moderate",
    srcs = ["test_consumer_restart.py"],
    filesystem = ":filesystem",
    # Disabled for thread sanitizer because of detected race (Ticket-246892)
    target_compatible_with = ["//quality/sanitizer/constraints:no_tsan"],
)
