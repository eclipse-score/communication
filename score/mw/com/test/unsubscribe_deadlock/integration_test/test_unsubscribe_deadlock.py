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

"""Integration test for unsubscribe_deadlock."""


def test_unsubscribe_deadlock(sut):
    """Test that Unsubscribe doesn't deadlock when called concurrently with GetSubscriptionState.
    
    This is a loopback test where both skeleton and proxy run in the same process.
    It tests that calling Unsubscribe() while GetSubscriptionState() is being called
    from a receive handler doesn't cause a deadlock.
    """
    with sut.start_process("./bin/unsubscribe_deadlock", cwd="/opt/unsubscribe_deadlock/") as process:
        assert process.wait_for_exit() == 0
