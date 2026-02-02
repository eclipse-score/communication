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


def test_generic_proxy(sut):
    """Test generic proxy functionality with sender and receiver."""
    # Sender runs for 30 cycles at 40ms intervals, Receiver receives 25 cycles
    with sut.start_process("./bin/generic_proxy -m send -t 40 -n 30", cwd="/opt/generic_proxy/") as sender:
        with sut.start_process("./bin/generic_proxy -m recv -n 25", cwd="/opt/generic_proxy/") as receiver:
            assert receiver.wait_for_exit() == 0
