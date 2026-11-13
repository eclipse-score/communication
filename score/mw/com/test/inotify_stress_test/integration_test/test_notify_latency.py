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

"""Integration test for the inotify stress test's notify-latency mode."""

from test_fixture import inotify_stress_test


def test_notify_latency(target):
    """Stress test for inotify: verifies that create/delete notifications on the shared
    base folder are dispatched to every watching worker within the configured timeout."""
    with inotify_stress_test(
        target,
        extra_args=["--mode", "notify-latency", "--notify-timeout-ms", "300"],
        wait_timeout=50,
    ):
        pass
