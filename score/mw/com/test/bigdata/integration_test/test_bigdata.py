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


def bigdata(target, mode, cycle_time=None, num_cycles=None, **kwargs):
    args = ["--mode", mode]
    if num_cycles is not None:
        args += ["-n", str(num_cycles)]
    if cycle_time is not None:
        args += ["-t", str(cycle_time)]
    wait_on_exit = num_cycles is not None
    return target.wrap_exec("bin/bigdata", args, cwd="/opt/bigdata", wait_on_exit=wait_on_exit, **kwargs)


def test_bigdata_exchange(target):
    """Test bigdata exchange between sender and receiver."""
    # Sender runs for continuous cycles at 40ms intervals, Receiver receives 25 cycles
    with bigdata(target, "send", cycle_time=40), bigdata(target, "recv", num_cycles=25, wait_timeout=120):
        pass
