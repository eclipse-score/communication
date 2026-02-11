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

"""Integration test for data_slots_read_only."""


def test_data_slots_read_only_basic(sut):
    """Test data slots read-only functionality."""
    with sut.start_process(
        "./bin/data_slots_read_only --mode recv --num-cycles 5 --should-modify-data-segment false",
        cwd="/opt/data_slots_read_only/",
    ) as receiver:
        with sut.start_process(
            "./bin/data_slots_read_only --mode send --cycle-time 10 --num-cycles 100 --should-modify-data-segment false",
            cwd="/opt/data_slots_read_only/",
        ) as sender:
            assert receiver.wait_for_exit() == 0

        assert sender.wait_for_exit() == 0
