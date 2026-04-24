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

"""Integration test for consumer_restart (graceful shutdown)."""

from consumer_restart_test_fixture import partial_restart_consumer

# Test configuration
NUMBER_RESTART_CYCLES = 3
KILL_CONSUMER = 0


def test_partial_restart_consumer_graceful(target):
    """Test consumer restart with graceful shutdown."""
    with partial_restart_consumer(target, NUMBER_RESTART_CYCLES, KILL_CONSUMER):
        pass
