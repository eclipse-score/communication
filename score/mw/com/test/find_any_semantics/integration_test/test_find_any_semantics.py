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

"""Integration tests for find any semantics service discovery."""


def test_find_any_semantics(sut):
    """Test service discovery with FindService (any semantics)."""
    with sut.start_process("./bin/service -t 250", cwd="/opt/ServiceApp/") as service_process:
        with sut.start_process("./bin/client -r 20 -b 50", cwd="/opt/ClientApp/") as client_process:
            assert client_process.wait_for_exit() == 0
