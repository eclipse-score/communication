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

"""Integration test: DynamicArray with PolymorphicOffsetPtrAllocator in shared memory."""


def dynamic_array_sct(target, **kwargs):
    return target.wrap_exec(
        "bin/dynamic_array_sct",
        [],
        cwd="/opt/dynamic_array_in_shm_test",
        wait_on_exit=True,
        **kwargs,
    )


def test_shared_memory_dynamic_array(target):
    """Start two instances of dynamic_array_sct to store and read from a shared DynamicArray in parallel."""
    with dynamic_array_sct(target), dynamic_array_sct(target):
        pass
