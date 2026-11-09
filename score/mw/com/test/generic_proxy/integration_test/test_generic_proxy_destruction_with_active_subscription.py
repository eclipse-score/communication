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

"""Integration test for destroying a generic_proxy with an active subscription.

Kept in its own test target so it runs on a fresh target environment. On QNX the
QEMU target is shared by all tests of a target, and a provider from a previous
test which did not shut down cleanly leaves a stale service offer behind.
"""


def generic_proxy(target, mode, cycle_time=None, num_cycles=None, **kwargs):
    args = ["--mode", mode]
    if num_cycles is not None:
        args += ["--num-cycles", str(num_cycles)]
    if cycle_time is not None:
        args += ["--cycle-time", str(cycle_time)]

    wait_on_exit = False
    if mode == "recv-active":
        wait_on_exit = True

    return target.wrap_exec(
        "bin/generic_proxy",
        args,
        cwd="/opt/generic_proxy",
        wait_on_exit=wait_on_exit,
        **kwargs,
    )


def test_generic_proxy_destruction_with_active_subscription(target):
    """Destroy a subscribed generic proxy after receiving samples."""
    # num_cycles = 0 signifies that the sender will run until interrupted
    with (
        generic_proxy(target, "send", cycle_time=40, num_cycles=0),
        generic_proxy(target, "recv-active", num_cycles=25, wait_timeout=15),
    ):
        pass
