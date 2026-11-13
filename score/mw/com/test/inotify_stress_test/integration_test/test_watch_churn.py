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

"""Integration test for the inotify stress test's watch-churn mode."""

from test_fixture import inotify_stress_test


def test_watch_churn(target):
    """Stress test for inotify: M processes concurrently create directories, files,
    set access rights, and add/remove watches over N cycles."""
    with inotify_stress_test(target, wait_timeout=50):
        pass
