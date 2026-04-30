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

"""Integration test for multiple_proxies."""


def multiple_proxies(target, mode, cycle_time=None, num_cycles=None, **kwargs):
    args = ["--mode", mode]
    if num_cycles is not None:
        args += ["--num-cycles", str(num_cycles)]
    if cycle_time is not None:
        args += ["--cycle-time", str(cycle_time)]
    wait_on_exit = num_cycles is not None
    return target.wrap_exec(
        "bin/multiple_proxies", args, cwd="/opt/multiple_proxies", wait_on_exit=wait_on_exit, **kwargs
    )


def test_multiple_proxies(target):
    """Test multiple proxy instances with sender and receiver."""
    with (
        multiple_proxies(target, "send", cycle_time=40),
        multiple_proxies(target, "recv", num_cycles=25, wait_timeout=60),
    ):
        pass
