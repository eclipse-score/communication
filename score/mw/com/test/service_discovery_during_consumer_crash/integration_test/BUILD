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

# NOTE: This target requires visibility fix in the main BUILD file
# The pkg_application visibility needs to include:
# "//score/mw/com/test/service_discovery_during_consumer_crash:__subpackages__"

integration_test(
    name = "service_discovery_during_consumer_crash",
    timeout = "moderate",
    srcs = ["test_service_discovery_during_consumer_crash.py"],
    filesystem = ":filesystem",
    # Disabled for thread sanitizer because of detected race (Ticket-246892)
    target_compatible_with = ["//quality/sanitizer/constraints:no_tsan"],
)

pkg_tar(
    name = "filesystem",
    deps = [
        "//score/mw/com/test/service_discovery_during_consumer_crash:service_discovery_during_consumer_crash-pkg",
    ],
)
