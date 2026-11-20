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

"""Smoke tests for the ETS SOME/IP server.

The tests check service discovery, one method call and one event.

Test setup:
  [Python test] --(SOME/IP UDP)--> [someipd] --(UDS)--> [ets]

Service under test (EnhancedTestabilityServiceLow2):
  Service ID    : 0x0103 (259)
  Instance ID   : 0x0091 (145)
  Major version : 2
"""

from __future__ import annotations

import asyncio

import pytest
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import (
    ETS_INSTANCE_ID,
    ETS_SERVICE_ID,
    EVENT_UINT8,
    METHOD_ECHO_UINT8,
    METHOD_TRIGGER_EVENT_UINT8,
    trigger,
)

EVENT_TIMEOUT = 15.0

pytestmark = pytest.mark.asyncio(loop_scope="class")


class TestEtsSmoke:
    async def test_service_is_discoverable(self, ets_client):
        assert await ets_client.is_available(), (
            f"ETS service 0x{ETS_SERVICE_ID:04x}/0x{ETS_INSTANCE_ID:04x} not available"
        )

    async def test_echo_uint8_method_call(self, ets_client):
        result = await asyncio.wait_for(
            ets_client.call_method(METHOD_ECHO_UINT8, b"\x2a"),
            timeout=10.0,
        )
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert result.payload[:1] == b"\x2a", f"Expected echo payload 0x2a, got {result.payload[:1].hex()}"

    async def test_event_uint8_received(self, ets_client, ets_eventgroup):
        received: list[tuple[int, bytes]] = []
        done = asyncio.Event()

        def _on_event(event_id: int, payload: bytes) -> None:
            if event_id == EVENT_UINT8:
                received.append((event_id, payload))
                done.set()

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)

        await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8, bytes([0, 3, 1]))

        try:
            await asyncio.wait_for(done.wait(), timeout=EVENT_TIMEOUT)
        except asyncio.TimeoutError:
            pytest.fail(
                f"No TestEventUINT8 (0x{EVENT_UINT8:04x}) notification received "
                f"within {EVENT_TIMEOUT:.0f} s after triggerEventUINT8"
            )
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

        event_id, _ = received[0]
        assert event_id == EVENT_UINT8, f"Expected event 0x{EVENT_UINT8:04x}, got 0x{event_id:04x}"
