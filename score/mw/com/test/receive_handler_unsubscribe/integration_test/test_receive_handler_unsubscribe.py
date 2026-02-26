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

"""Integration test for receive_handler_unsubscribe."""


def test_receive_handler_unsubscribe(sut):
    """Test that receive handler is no longer invoked after unsubscribe.
    
    This is a loopback test where both skeleton and proxy run in the same process.
    The skeleton publishes events and the proxy receives notifications via a callback.
    When Unsubscribe is called, it verifies the callback is no longer invoked.
    """
    with sut.start_process("./bin/receive_handler_unsubscribe -n 100 -t 10", cwd="/opt/receive_handler_unsubscribe/") as process:
        assert process.wait_for_exit() == 0
