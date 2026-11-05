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

"""Integration test for the minimal, standalone inotify_event_loss reproducer.

Unlike the other scenarios in this directory, this one exercises
//score/mw/com/test/inotify_stress_test:inotify_event_loss directly (not
:inotify_stress_test) - a self-contained, dependency-free binary suitable for
sharing as-is with QNX support, reproducing a real inotify event-loss/reordering
bug found via the burst-loss stress test.
"""


def test_event_loss(target):
    """Repeatedly has a forked "event producer" process create, then remove, many
    consecutively-named files in a watched directory, and verifies that the parent
    process observes every create/delete notification, in the correct order, within
    a grace period after each cycle ends."""
    with target.wrap_exec(
        "bin/inotify_event_loss",
        [
            "--cycles",
            "20",
            "--file-count",
            "500",
            "--check-delay-ms",
            "500",
        ],
        cwd="/opt/InotifyEventLossApp",
        wait_on_exit=True,
        wait_timeout=120,
    ):
        pass
