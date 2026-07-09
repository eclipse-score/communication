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
from partial_restart_methods_consumer_test_fixture import methods_consumer_restart

# Test configuration
NUMBER_RESTART_CYCLES = 10
KILL_CONSUMER = 0


def test_partial_restart_methods_consumer_graceful(target):
    """Test that a consumer can gracefully restart and continue calling methods on a provider."""
    with methods_consumer_restart(target, NUMBER_RESTART_CYCLES, KILL_CONSUMER):
        pass
