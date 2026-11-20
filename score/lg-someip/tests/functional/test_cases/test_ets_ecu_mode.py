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

"""Tests for the offer that depends on the ECU mode.

This offer is not active. EcuMode<ServiceType> is complete, but EnhancedTestabilityService
does not use it. The test container does not have SecEcuModeProxy.
Thus both tests are skipped. They are ready for use when EnhancedTestabilityService uses EcuMode.

Expected behavior when the offer depends on the ECU mode:
  ENGINEERING mode  → skeleton calls OfferService()   → ETS is discoverable
  NORMAL/PRODUCTION → skeleton calls StopOfferService() → ETS is not discoverable
"""

from __future__ import annotations

import pytest

pytestmark = pytest.mark.asyncio(loop_scope="class")

_SKIP_REASON = "EcuMode not wired into EnhancedTestabilityService; SecEcuModeProxy not available in test container"


class TestEcuMode:
    @pytest.mark.skip(reason=_SKIP_REASON)
    async def test_service_offered_only_in_engineering_mode(self, ets_client):
        # When the service uses EcuMode and the mode is ENGINEERING:
        #   assert await ets_client.is_available() is True
        # When the mode is NORMAL / PRODUCTION:
        #   assert await ets_client.is_available() is False
        pytest.fail("EcuMode not wired into EnhancedTestabilityService")

    @pytest.mark.skip(reason=_SKIP_REASON)
    async def test_mode_oscillation_no_corruption(self, ets_client):
        # When the service uses EcuMode: change the ECU mode 100 times in 10 s.
        # Expected: no crash. Subscribers never see a partial offer.
        #   await ets_client.is_available() does not raise and returns True or False.
        pytest.fail("EcuMode not wired into EnhancedTestabilityService")
