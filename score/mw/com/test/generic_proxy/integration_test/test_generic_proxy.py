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

"""Integration test for generic_proxy."""


def generic_proxy(target, mode, cycle_time=None, num_cycles=None, **kwargs):
    args = ["--mode", mode]
    if num_cycles is not None:
        args += ["--num-cycles", str(num_cycles)]
    if cycle_time is not None:
        args += ["--cycle-time", str(cycle_time)]

    wait_on_exit = False
    if mode == "recv":
        wait_on_exit = True

    return target.wrap_exec("bin/generic_proxy", args, cwd="/opt/generic_proxy", wait_on_exit=wait_on_exit, **kwargs)


def test_generic_proxy(target):
    """Test generic proxy functionality with sender and receiver."""
    # num_cycles = 0 signifies that the sender will run untill interrupted
    with generic_proxy(target, "send", cycle_time=40, num_cycles=0), generic_proxy(target, "recv", num_cycles=25, wait_timeout=15):
        pass
