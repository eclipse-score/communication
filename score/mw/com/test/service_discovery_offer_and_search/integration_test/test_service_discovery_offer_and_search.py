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

"""Integration tests for service discovery offer and search."""


def test_service_discovery_offer_and_search(sut):
    """Test service discovery where service offers first, then client searches."""
    with sut.start_process("./bin/service -t 250", cwd="/opt/ServiceApp/") as service_process:
        with sut.start_process("./bin/client", cwd="/opt/ClientApp/") as client_process:
            assert client_process.wait_for_exit() == 0
