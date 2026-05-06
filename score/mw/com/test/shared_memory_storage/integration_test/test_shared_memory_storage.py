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


def shared_memory_storage(target, mode, **kwargs):
    args = ["--mode", mode]
    return target.wrap_exec(
        "bin/shared_memory_storage", args, cwd="/opt/shared_memory_storage", wait_on_exit=True, **kwargs
    )


def test_shared_memory_storage(target):
    """Test shared memory storage functionality with sender and receiver."""
    with shared_memory_storage(target, "send"), shared_memory_storage(target, "recv", wait_timeout=15):
        pass
