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

load("//quality/integration_testing:integration_testing.bzl", "integration_test")

integration_test(
    name = "provider_restart_max_subscribers_no_proxy",
    timeout = "moderate",
    srcs = [
        "provider_restart_max_subscribers_test_fixture.py",
        "test_provider_restart_max_subscribers_no_proxy.py",
    ],
    filesystem = "//score/mw/com/test/partial_restart/provider_restart_max_subscribers:provider_restart_max_subscribers-pkg",
    # Disabled for thread sanitizer because of detected race (Ticket-246892)
    target_compatible_with = ["//quality/sanitizer/constraints:no_tsan"],
)

integration_test(
    name = "provider_restart_max_subscribers_with_proxy",
    timeout = "moderate",
    srcs = [
        "provider_restart_max_subscribers_test_fixture.py",
        "test_provider_restart_max_subscribers_with_proxy.py",
    ],
    filesystem = "//score/mw/com/test/partial_restart/provider_restart_max_subscribers:provider_restart_max_subscribers-pkg",
    # Disabled for thread sanitizer because of detected race (Ticket-246892)
    target_compatible_with = ["//quality/sanitizer/constraints:no_tsan"],
)
