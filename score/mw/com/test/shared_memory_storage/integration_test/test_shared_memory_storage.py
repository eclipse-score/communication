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

"""Integration test for shared_memory_storage."""


def test_shared_memory_storage(sut):
    """Test shared memory storage functionality with sender and receiver."""
    # Sender and receiver test memory storage mechanisms
    with sut.start_process("./bin/shared_memory_storage -m send -t 40 -n 30", cwd="/opt/shared_memory_storage/") as sender:
        with sut.start_process("./bin/shared_memory_storage -m recv -n 25", cwd="/opt/shared_memory_storage/") as receiver:
            assert receiver.wait_for_exit() == 0
