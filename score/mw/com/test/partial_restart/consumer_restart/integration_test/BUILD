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
    name = "consumer_restart_graceful",
    timeout = "moderate",
    srcs = [
        "consumer_restart_test_fixture.py",
        "test_consumer_restart_graceful.py",
    ],
    filesystem = "//score/mw/com/test/partial_restart/consumer_restart:consumer_restart-pkg",
    # Disabled for thread sanitizer because of detected race (Ticket-246892)
    target_compatible_with = ["//quality/sanitizer/constraints:no_tsan"],
)

integration_test(
    name = "consumer_restart_kill",
    timeout = "moderate",
    srcs = [
        "consumer_restart_test_fixture.py",
        "test_consumer_restart_kill.py",
    ],
    filesystem = "//score/mw/com/test/partial_restart/consumer_restart:consumer_restart-pkg",
    # Flaky: after SIGKILL, CreateNewClient may not reach kReady within its
    # 500 ms budget under ASAN/LSan, causing the fire-and-forget
    # RegisterEventNotificationMessage to fail.  The consumer then waits
    # for event notifications that were never registered.
    flaky = True,
    # Disabled for thread sanitizer because of detected race (Ticket-246892)
    target_compatible_with = ["//quality/sanitizer/constraints:no_tsan"],
)
