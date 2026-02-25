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

"""Integration tests for receive handler usage."""

# NOTE: This test currently fails in Docker-based integration testing (exit code 135 - SIGBUS).
# The receive_handler_usage application is a loopback test that creates both skeleton and proxy
# in the same process with 218MB of shared memory. This pattern fails in Docker isolation with
# memory mapping errors. This test is designed as a unit test and runs successfully in SCTF
# with use_sandbox=True, but is not suitable for Docker-based integration testing.


def test_receive_handler_usage(sut):
    """Test that mw::com APIs can be called from within EventReceiveHandler without deadlocks."""
    with sut.start_process("./bin/receive_handler_usage", cwd="/opt/receive_handler_usage/") as process:
        assert process.wait_for_exit() == 0
