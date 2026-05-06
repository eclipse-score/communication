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

"""Integration test for provider_restart (kill with proxy)."""

from provider_restart_test_fixture import partial_restart_provider

# Test configuration
NUMBER_RESTART_CYCLES = 3
CREATE_PROXY = 1
KILL_PROVIDER = 1


def test_provider_restart_kill(target):
    """Test provider restart with kill (SIGKILL) signal."""
    with partial_restart_provider(target, NUMBER_RESTART_CYCLES, CREATE_PROXY, KILL_PROVIDER):
        pass
