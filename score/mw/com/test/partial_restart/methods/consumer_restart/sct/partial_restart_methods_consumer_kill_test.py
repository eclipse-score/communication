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
import sctf
from partial_restart_methods_consumer_test_fixture import MethodsConsumerRestart

# Test configuration
NUMBER_RESTART_CYCLES = 10
KILL_CONSUMER = 1


def test_partial_restart_methods_consumer_kill(basic_sandbox):
    """Test that a consumer can restart after being killed and continue calling methods on a provider."""
    with MethodsConsumerRestart(basic_sandbox, NUMBER_RESTART_CYCLES, KILL_CONSUMER):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
