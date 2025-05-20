# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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
from partial_restart_consumer_test_fixture import PartialRestartConsumer

# Test configuration
NUMBER_RESTART_CYCLES = 10
KILL_PROVIDER = 0


def test_partial_restart_consumer_graceful(environment):
    with PartialRestartConsumer(environment, NUMBER_RESTART_CYCLES, KILL_PROVIDER):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
