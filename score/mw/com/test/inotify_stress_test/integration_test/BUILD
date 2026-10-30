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
    name = "test_watch_churn",
    srcs = [
        "test_fixture.py",
        "test_watch_churn.py",
    ],
    filesystem = "//score/mw/com/test/inotify_stress_test:inotify-stress-test-pkg",
)

integration_test(
    name = "test_notify_latency",
    srcs = [
        "test_fixture.py",
        "test_notify_latency.py",
    ],
    filesystem = "//score/mw/com/test/inotify_stress_test:inotify-stress-test-pkg",
)

integration_test(
    name = "test_burst_loss",
    srcs = [
        "test_burst_loss.py",
        "test_fixture.py",
    ],
    filesystem = "//score/mw/com/test/inotify_stress_test:inotify-stress-test-pkg",
)

integration_test(
    name = "test_event_loss",
    srcs = [
        "test_event_loss.py",
    ],
    filesystem = "//score/mw/com/test/inotify_stress_test:inotify-event-loss-pkg",
)
