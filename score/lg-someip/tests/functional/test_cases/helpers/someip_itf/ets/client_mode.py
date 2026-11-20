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

"""Controls the ETS client mode of the DUT through its server methods.

The DUT client uses the ETS instance of the tester.

``activate_and_find`` and ``subscribe`` stop the test with ``pytest.fail``, not with an ``AssertionError``.
Thus a test with ``xfail(raises=AssertionError)`` fails when the client mode does not start.
"""

from __future__ import annotations

import asyncio
from collections.abc import Awaitable, Callable

import pytest
from someip_itf.codec import SdEntry, SdEntryType
from someip_itf.ets.calls import assert_ok, call_echo
from someip_itf.ets.client import MessageType
from someip_itf.ets.constants import (
    ETS_MAJOR_VERSION,
    ETS_MINOR_VERSION,
    METHOD_CLIENT_SERVICE_ACTIVATE,
    METHOD_CLIENT_SERVICE_DEACTIVATE,
    METHOD_CLIENT_SERVICE_SUBSCRIBE_EVENTGROUP,
    METHOD_ECHO_UINT8_ARRAY,
    trigger,
)
from someip_itf.ets.payloads import pack_array, unpack_array
from someip_itf.ets.provider import TesterEts

FIND_TIMEOUT = 15.0  # the DUT can start the activation only after it completes the previous request
SD_TIMEOUT = 10.0  # time from an offer, or from the subscribe start delay, to the Subscribe of the DUT
SUBSCRIBE_START = 1  # clientServiceSubscribeEventgroup start delay (s); the tester sends its offer in this time
SUBSCRIBE_DURATION = 8  # clientServiceSubscribeEventgroup duration (s); longer than all steps of one subscribe test
_DELIVERY_ATTEMPTS = 10
_DELIVERY_INTERVAL = 0.5
_ANY_MAJOR_VERSION = 0xFF
_ANY_MINOR_VERSION = 0xFFFFFFFF


def is_eventgroup(eventgroup_id: int) -> Callable[[SdEntry], bool]:
    """Match a Subscribe, not a StopSubscribe, for *eventgroup_id*."""
    return lambda entry: entry.ttl > 0 and entry.eventgroup_id == eventgroup_id


def assert_echo_requests(requests, sent: list[list[int]]) -> None:
    """Make sure that *requests* are echoUINT8Array calls with exactly the arrays in *sent*, in any order."""
    assert all(request.method_id == METHOD_ECHO_UINT8_ARRAY for request in requests), (
        f"the tester's ETS got methods {[request.method_id for request in requests]}, not echoUINT8Array"
    )
    assert sorted(request.payload for request in requests) == sorted(pack_array(values) for values in sent), (
        f"the tester's ETS got echo payloads {[request.payload.hex() for request in requests]}"
    )


class ClientModeSession:
    """One client-mode run of a test: the tester ETS, and the DUT client that ``ets_client`` controls.

    A DUT can run clientServiceSubscribeEventgroup until its duration ends and accept no other subscription
    in that time. Thus ``close`` waits until that duration ends.
    """

    def __init__(self, ets_client, tester: TesterEts) -> None:
        self.ets_client = ets_client
        self.tester = tester
        self._subscription_ends = 0.0

    async def activate_and_find(self, delay: int = 0) -> SdEntry:
        """Trigger clientServiceActivate, check the version fields of the FindService from the DUT, and return it."""
        await trigger(self.ets_client, METHOD_CLIENT_SERVICE_ACTIVATE, bytes([delay]))
        find = await self.tester.wait_for(SdEntryType.FindService, FIND_TIMEOUT)
        if find is None:
            pytest.fail("the DUT sent no FindService for 0x0103/0x00F4 after clientServiceActivate")
        if find.major_version not in (_ANY_MAJOR_VERSION, ETS_MAJOR_VERSION):
            pytest.fail(f"FindService asks for major version {find.major_version:#x}, not 0xff or the ETS's")
        if find.minver_or_counter not in (_ANY_MINOR_VERSION, ETS_MINOR_VERSION):
            pytest.fail(f"FindService asks for minor version {find.minver_or_counter:#x}, not 0xffffffff or the ETS's")
        return find

    async def subscribe(
        self, eventgroup_id: int, duration: int = SUBSCRIBE_DURATION, activate_delay: int = 0
    ) -> SdEntry:
        """Activate, trigger clientServiceSubscribeEventgroup, offer, and return the Subscribe for *eventgroup_id*."""
        await self.activate_and_find(activate_delay)
        await trigger(
            self.ets_client,
            METHOD_CLIENT_SERVICE_SUBSCRIBE_EVENTGROUP,
            bytes([SUBSCRIBE_START, duration]),
        )
        self._subscription_ends = asyncio.get_running_loop().time() + SUBSCRIBE_START + duration + 1.0
        self.tester.start()
        subscribe = await self.tester.wait_for(SdEntryType.Subscribe, SD_TIMEOUT, is_eventgroup(eventgroup_id))
        if subscribe is None:
            pytest.fail(f"the DUT sent no Subscribe for eventgroup {eventgroup_id} after the tester's offer")
        return subscribe

    async def deliver(self, send: Callable[[], None], getter: Callable[..., Awaitable], expected: int):
        """Send an event with *send* until *getter* reads *expected* back, and return the last read.

        The function sends again because the DUT can still join the group or apply the Ack after the first event.
        """
        result = None
        for _ in range(_DELIVERY_ATTEMPTS):
            send()
            result = await getter(self.ets_client)
            if result.message_type == MessageType.RESPONSE and result.payload[:1] == bytes([expected]):
                return result
            await asyncio.sleep(_DELIVERY_INTERVAL)
        return result

    async def echo_through_tester(self, values: list[int]) -> None:
        """Call clientServiceCallEchoUINT8Array and make sure that the tester ETS answers with *values*."""
        answered = len(self.tester.requests)
        result = await call_echo(self.ets_client, values)
        assert_ok(result)
        assert unpack_array(result.payload) == values, f"client echo returned {unpack_array(result.payload)}"
        assert_echo_requests(self.tester.requests[answered:], [values])

    async def close(self) -> None:
        """Deactivate the DUT client, wait until an active subscription ends if necessary, and stop the tester ETS."""
        try:
            await trigger(self.ets_client, METHOD_CLIENT_SERVICE_DEACTIVATE, bytes([0]))
            remaining = self._subscription_ends - asyncio.get_running_loop().time()
            if remaining > 0:
                await asyncio.sleep(remaining)
        finally:
            await asyncio.to_thread(self.tester.stop)
