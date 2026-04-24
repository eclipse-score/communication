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

"""Integration test for receive_handler_unsubscribe."""


def receive_handler_unsubscribe(target, cycle_time=None, num_cycles=None, **kwargs):
    args = []
    if num_cycles is not None:
        args += ["-n", str(num_cycles)]
    if cycle_time is not None:
        args += ["-t", str(cycle_time)]
    wait_on_exit = num_cycles is not None
    return target.wrap_exec(
        "bin/receive_handler_unsubscribe",
        args,
        cwd="/opt/receive_handler_unsubscribe",
        wait_on_exit=wait_on_exit,
        **kwargs,
    )


def test_receive_handler_unsubscribe(target):
    """Test that receive handler is no longer invoked after unsubscribe.

    This is a loopback test where both skeleton and proxy run in the same process.
    The skeleton publishes events and the proxy receives notifications via a callback.
    When Unsubscribe is called, it verifies the callback is no longer invoked.
    """
    with receive_handler_unsubscribe(target, cycle_time=10, num_cycles=100, wait_timeout=60):
        pass
