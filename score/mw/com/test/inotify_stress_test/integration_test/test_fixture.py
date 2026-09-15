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

"""Shared helper for launching the inotify stress test binary."""


def inotify_stress_test(target, extra_args=None, **kwargs):
    args = [
        "--num-processes",
        "10",
        "--cycles",
        "300",
        "--base-uid",
        "2000",
        "--base-gid",
        "2000",
    ] + (extra_args or [])
    return target.wrap_exec(
        "bin/inotify_stress_test",
        args,
        cwd="/opt/InotifyStressTestApp",
        wait_on_exit=True,
        **kwargs,
    )
