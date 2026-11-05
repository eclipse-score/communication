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

"""Integration test for the inotify stress test's burst-loss mode."""

from test_fixture import inotify_stress_test


def test_burst_loss(target):
    """Stress test for inotify: verifies that no create/delete notification is lost for a
    burst of many files created (then removed) back-to-back in one shot, for every watching
    worker. Models production reports of processes that watch many events occasionally
    missing one, suspected to be caused by an inotify event queue overflow under load."""
    with inotify_stress_test(
        target,
        cycles=20,
        extra_args=["--mode", "burst-loss", "--burst-file-count", "500", "--burst-check-delay-ms", "500"],
        wait_timeout=120,
    ):
        pass
