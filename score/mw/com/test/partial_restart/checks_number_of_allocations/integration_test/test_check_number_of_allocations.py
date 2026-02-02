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

"""Integration test for checks_number_of_allocations."""


def test_checks_number_of_allocations(sut):
    """Test checks number of allocations functionality."""
    with sut.start_process(
        "./bin/check_number_of_allocations",
        cwd="/opt/check_number_of_allocations",
    ) as process:
        assert process.wait_for_exit() == 0
