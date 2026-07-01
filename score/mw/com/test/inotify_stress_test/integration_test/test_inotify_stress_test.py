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

"""Integration test for the inotify stress test."""


def inotify_stress_test(target, **kwargs):
    args = [
        "--num-processes", "10",
        "--cycles", "300",
        "--base-uid", "2000",
        "--base-gid", "2000",
    ]
    return target.wrap_exec(
        "bin/inotify_stress_test",
        args,
        cwd="/opt/InotifyStressTestApp",
        wait_on_exit=True,
        **kwargs,
    )


def test_inotify_stress(target):
    """Stress test for inotify: M processes concurrently create directories, files,
    set access rights, and add/remove watches over N cycles."""
    with inotify_stress_test(target, wait_timeout=50):
        pass
