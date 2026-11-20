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

"""ETS fixtures that override the conftest fixtures in the module that imports them."""

from __future__ import annotations

import pytest

from someip_itf.dut import relaunch_dut


@pytest.fixture(scope="class")
def ets_dut(ets_dut, request):
    """Restart the DUT for each test class. Use it when the classes stop the DUT on purpose.

    The session-scoped ``ets_dut`` fixture starts the DUT one time only.
    """
    relaunch_dut(ets_dut, request.config)
    return ets_dut
