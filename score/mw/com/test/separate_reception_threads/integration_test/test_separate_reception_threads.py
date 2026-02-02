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

"""Integration test for separate_reception_threads."""


def test_separate_reception_threads(sut):
    """Test separate reception threads functionality.
    
    NOTE: The main application code for this test is disabled (#if 0) and just returns
    EXIT_SUCCESS immediately. This test verifies the stub runs without error.
    """
    with sut.start_process("./bin/separate_reception_threads", cwd="/opt/separate_reception_threads/") as process:
        assert process.wait_for_exit() == 0
