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

"""Event publication tests.

A triggerEvent method (start, duration, debounce) makes the DUT send floor(duration / debounce) + 1 events.
The counter starts at 0 and increases by 1 for each event. The array variant sends [0], then [0, 1], and so on.
The methods are fire-and-forget, so a test waits for the events and not for a response.

The payload of TestEventUINT8Array is a 4-byte big-endian length, followed by that number of bytes.

The DUT sends the events of eventgroup 6 to EVENTGROUP_MULTICAST_GROUP when two clients subscribe (threshold 2).
"""

from __future__ import annotations

import asyncio
import types

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.constants import (
    EVENT_UINT8,
    EVENT_UINT8_ARRAY,
    EVENT_UINT8_MULTICAST,
    EVENTGROUP_MULTICAST,
    EVENTGROUP_MULTICAST_GROUP,
    EVENTGROUP_UNICAST,
    METHOD_TRIGGER_EVENT_UINT8,
    METHOD_TRIGGER_EVENT_UINT8_ARRAY,
    METHOD_TRIGGER_EVENT_UINT8_MULTICAST,
    trigger,
)
from someip_itf.ets.payloads import trigger_request, unpack_array

_LGSOMEIP_4_XFAIL_REASON = (
    "[lg-someip #4] the daemon matches a Subscribe to an existing subscription by IP address only, so a second "
    "subscriber endpoint on the same IP address gets no route"
)

pytestmark = pytest.mark.asyncio(loop_scope="class")


class TestCyclicEvents:
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_147"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_event_count_and_values(self, ets_client, ets_eventgroup):
        """(variant): triggerEventUINT8 sends TestEventUINT8 counters 0..5 in sequence.

        Variant: the test uses eventgroup 5, because the DUT does not have eventgroup 2.
        The test does not check the SubscribeEventgroupAck.
        """
        # trigger(start=1, duration=10, debounce=2) → floor(10/2)+1 = 6 events (0,1,2,3,4,5)
        # With start=1, the worker waits 1 s before it sends the first event.
        EXPECTED = [0, 1, 2, 3, 4, 5]
        received: list[int] = []
        done = asyncio.Event()

        def _on_event(event_id: int, payload: bytes) -> None:
            if event_id == EVENT_UINT8:
                received.append(payload[0])
                if len(received) >= len(EXPECTED):
                    done.set()

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        await asyncio.sleep(1)
        try:
            await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8, trigger_request(1, 10, 2))
            await asyncio.wait_for(done.wait(), timeout=15.0)
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

        assert received == EXPECTED, f"Expected counters {EXPECTED}, got {received}"

    # Single shot: duration=0 gives exactly one event (value 0).
    async def test_single_shot(self, ets_client, ets_eventgroup):
        received: list[int] = []
        done = asyncio.Event()

        def _on_event(event_id: int, payload: bytes) -> None:
            if event_id == EVENT_UINT8:
                received.append(payload[0])
                done.set()

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        # `subscribe_eventgroup` does not wait for the subscription acknowledgement.
        # A short delay makes sure that the test receives the first event.
        await asyncio.sleep(1)
        try:
            await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8, trigger_request(0, 0, 1))
            # floor(0/1)+1 = 1. Wait 5 s, then make sure that exactly one event arrived.
            await asyncio.wait_for(done.wait(), timeout=5.0)
        except asyncio.TimeoutError:
            pass
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

        assert len(received) == 1, f"Expected 1 event, got {len(received)}: {received}"
        assert received[0] == 0, f"Expected counter 0, got {received[0]}"

    # debounceTime=0: the service must not send a flood of events or crash.
    async def test_debounce_zero_no_crash(self, ets_client, ets_eventgroup):
        event_count = [0]

        def _on_event(event_id: int, payload: bytes) -> None:  # noqa: ARG001
            if event_id == EVENT_UINT8:
                event_count[0] += 1

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        try:
            await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8, trigger_request(0, 2, 0))
            await asyncio.sleep(3.0)
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

        # A flood gives hundreds of events in 3 s. A reasonable limit catches it.
        assert event_count[0] < 100, f"Possible event flood: {event_count[0]} events in 3 s with debounceTime=0"
        assert await ets_client.is_available(), "Service unavailable after debounceTime=0 trigger"

    async def test_late_subscriber_misses_early_events(self, ets_client, ets_eventgroup):
        # Each debounceTime tick is 100 ms, so debounceTime=20 gives 2 s for each event.
        # trigger(0,60,20) sends events at t=0,2,4,6 (values 0,1,2,3).
        # trigger() returns immediately, so the 5 s sleep puts the subscribe at t≈5.
        # The event at t=6 (value=3) is still pending.
        # The burst ends at t=6, when the test receives its first event.
        # Thus no events from this burst go into the subsequent tests.
        received: list[int] = []
        done = asyncio.Event()

        def _on_event(event_id: int, payload: bytes) -> None:
            if event_id == EVENT_UINT8:
                received.append(payload[0])
                done.set()

        await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8, trigger_request(0, 60, 20))
        await asyncio.sleep(5.0)

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        try:
            await asyncio.wait_for(done.wait(), timeout=10.0)
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

        assert len(received) > 0, "Late subscriber received no events"
        assert received[0] > 0, f"First received counter should be > 0 (late join), got {received[0]}"

    async def test_counter_resets_on_second_burst(self, ets_client, ets_eventgroup):
        # First burst: trigger(0,2,1) → 3 events (values 0,1,2).
        first: list[int] = []
        first_done = asyncio.Event()

        def _collect_first(event_id: int, payload: bytes) -> None:
            if event_id == EVENT_UINT8:
                first.append(payload[0])
                if len(first) >= 3:
                    first_done.set()

        ets_client.register_callback(_collect_first)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        # `subscribe_eventgroup` does not wait for the subscription acknowledgement.
        # A short delay makes sure that the test receives the first event.
        await asyncio.sleep(1)
        try:
            await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8, trigger_request(0, 2, 1))
            await asyncio.wait_for(first_done.wait(), timeout=10.0)
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

        assert first == [0, 1, 2], f"First burst: expected [0,1,2], got {first}"

        # A short pause lets the first burst end before the second trigger.
        await asyncio.sleep(0.5)

        # Second burst: same parameters. The counter of each trigger starts again at 0.
        second: list[int] = []
        second_done = asyncio.Event()

        def _collect_second(event_id: int, payload: bytes) -> None:
            if event_id == EVENT_UINT8:
                second.append(payload[0])
                second_done.set()

        ets_client.register_callback(_collect_second)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        # `subscribe_eventgroup` does not wait for the subscription acknowledgement.
        # A short delay makes sure that the test receives the first event.
        await asyncio.sleep(1)
        try:
            await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8, trigger_request(0, 2, 1))
            await asyncio.wait_for(second_done.wait(), timeout=10.0)
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

        assert second[0] == 0, f"Second burst should start at counter 0, got {second[0]}"

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_148"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_array_event_growing_payload(self, ets_client, ets_eventgroup):
        """(variant): each TestEventUINT8Array event has one more element than the event before it.

        Variant: the test uses eventgroup 5, because the DUT does not have eventgroup 2.
        The test does not check the SubscribeEventgroupAck.
        """
        # trigger(0,3,1) → 4 array events; expected payloads: [0],[0,1],[0,1,2],[0,1,2,3]
        BURST_SIZE = 4
        arrays: list[list[int]] = []
        done = asyncio.Event()

        def _on_event(event_id: int, payload: bytes) -> None:
            if event_id == EVENT_UINT8_ARRAY:
                arrays.append(unpack_array(payload))
                if len(arrays) >= BURST_SIZE:
                    done.set()

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        # Wait for the subscription acknowledgement. 2 s makes the full-suite run reliable.
        await asyncio.sleep(2)
        try:
            await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8_ARRAY, trigger_request(0, 3, 1))
            await asyncio.wait_for(done.wait(), timeout=10.0)
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

        assert len(arrays) == BURST_SIZE, f"Expected {BURST_SIZE} array events, got {len(arrays)}"
        for i, arr in enumerate(arrays):
            expected = list(range(i + 1))
            assert arr == expected, f"Event {i}: expected {expected}, got {arr}"

    # Eventgroup isolation: a subscription to group 5 must not receive group 6 events.
    async def test_eventgroup_5_isolation_from_group_6(self, ets_client, ets_eventgroup):
        group6_events: list[int] = []

        def _on_event(event_id: int, payload: bytes) -> None:
            if event_id == EVENT_UINT8_MULTICAST:
                group6_events.append(payload[0])

        ets_client.register_callback(_on_event)
        # Subscribe to eventgroup 5 only.
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        try:
            # Trigger a multicast burst (eventgroup 6). The group 5 subscriber must not receive it.
            await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8_MULTICAST, trigger_request(0, 3, 1))
            # Monitor for 6 s, which is longer than the full burst.
            await asyncio.sleep(6.0)
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

        assert group6_events == [], (
            f"Received {len(group6_events)} group-6 events on a group-5 subscription: {group6_events}"
        )


# ---------------------------------------------------------------------------
# Eventgroup 6 changes between unicast and multicast delivery
# ---------------------------------------------------------------------------

_EVENTGROUP_5 = types.SimpleNamespace(id=EVENTGROUP_UNICAST)
_EVENTGROUP_6 = types.SimpleNamespace(id=EVENTGROUP_MULTICAST)
#: The counters one burst sends: trigger_request(0, 3, 1).
_COUNTERS = [0, 1, 2, 3]
#: Time for the server to process a Subscribe or StopSubscribe.
_SD_SETTLE = 0.5


async def _burst(clients, method_id: int = METHOD_TRIGGER_EVENT_UINT8_MULTICAST) -> None:
    """Clear the records of the clients and trigger one burst. The wait is longer than the burst to catch duplicates."""
    for client in clients:
        client.notifications.clear()
    await trigger(clients[0], method_id, trigger_request(0, _COUNTERS[-1], 1))
    await asyncio.sleep(4.0)


async def _unsubscribe_all(clients) -> None:
    """Stop all subscriptions of a case, so that the next case starts with no subscriber."""
    for client in clients:
        client.unsubscribe_eventgroup(_EVENTGROUP_5)
        client.unsubscribe_eventgroup(_EVENTGROUP_6)
    await asyncio.sleep(_SD_SETTLE)


@pytest.fixture
def eventgroup6() -> types.SimpleNamespace:
    """The group in a SubscribeAck for eventgroup 6, and what each of two subscribers receives of one burst.

    The lg-someip configuration gives eventgroup 6 ``EVENTGROUP_MULTICAST_GROUP`` and threshold 2.
    Thus two subscribers share the group.
    """
    return types.SimpleNamespace(group=EVENTGROUP_MULTICAST_GROUP, shared=([], _COUNTERS))


class TestMulticastDelivery:
    """Eventgroup 6 follows its multicast threshold as its subscriber count changes.

    ``ets_client`` alone always gets unicast.
    When ``ets_client2`` subscribes, both clients get the path that :func:`eventgroup6` gives.
    Each client records if an event came over unicast or over the group.
    Thus a case makes sure that each client receives each event exactly once, over the expected path and from the
    offered endpoint.
    """

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_150"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_4_XFAIL_REASON)
    async def test_delivery_follows_the_subscriber_count(self, ets_client, ets_client2, eventgroup6):
        """(variant): one subscriber gets unicast, two get the configured path, and an unsubscribe reverses it."""
        clients = [ets_client, ets_client2]
        try:
            ack = await ets_client.subscribe_and_wait_for_ack(_EVENTGROUP_6)
            group = ets_client.ack_multicast_group(ack)
            assert group == eventgroup6.group, "an Ack names the configured group, also below the threshold"
            await _burst(clients)
            assert ets_client.received_counters(EVENT_UINT8_MULTICAST) == (_COUNTERS, [])
            assert ets_client2.received_counters(EVENT_UINT8_MULTICAST) == ([], [])

            await ets_client2.subscribe_and_wait_for_ack(_EVENTGROUP_6)
            # This wait has two cyclic Offers. Both clients answer each Offer with a Subscribe that renews it.
            await asyncio.sleep(2.5)
            await _burst(clients)
            for client in clients:
                assert client.received_counters(EVENT_UINT8_MULTICAST) == eventgroup6.shared

            ets_client2.unsubscribe_eventgroup(_EVENTGROUP_6)
            await asyncio.sleep(_SD_SETTLE)
            await _burst(clients)
            assert ets_client.received_counters(EVENT_UINT8_MULTICAST) == (_COUNTERS, [])
            assert ets_client2.received_counters(EVENT_UINT8_MULTICAST) == ([], [])

            ets_client.unsubscribe_eventgroup(_EVENTGROUP_6)
            await asyncio.sleep(_SD_SETTLE)
            await _burst(clients)
            for client in clients:
                assert client.received_counters(EVENT_UINT8_MULTICAST) == ([], [])
        finally:
            await _unsubscribe_all(clients)

    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_4_XFAIL_REASON)
    async def test_an_expired_subscription_returns_to_unicast(self, ets_client, ets_client2, eventgroup6):
        """When the TTL of a second subscription expires, the first subscriber goes back to unicast."""
        clients = [ets_client, ets_client2]
        try:
            await ets_client.subscribe_and_wait_for_ack(_EVENTGROUP_6)
            await ets_client2.subscribe_and_wait_for_ack(_EVENTGROUP_6, ttl=5)
            # From here, ets_client2 does not renew or subscribe again on an Offer. Thus its 5 s TTL expires.
            ets_client2.stop_maintaining_subscription(_EVENTGROUP_6)
            await _burst(clients)
            assert ets_client.received_counters(EVENT_UINT8_MULTICAST) == eventgroup6.shared
            # The burst took 4 s. This wait is longer than the TTL and the 200 ms TTL tick of the server.
            await asyncio.sleep(2.5)
            await _burst(clients)
            assert ets_client.received_counters(EVENT_UINT8_MULTICAST) == (_COUNTERS, [])
            assert ets_client2.received_counters(EVENT_UINT8_MULTICAST) == ([], [])
        finally:
            await _unsubscribe_all(clients)

    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_4_XFAIL_REASON)
    async def test_the_threshold_applies_per_eventgroup(self, ets_client, ets_client2, eventgroup6):
        """Eventgroup 5 has no threshold, so its two subscribers keep unicast. Eventgroup 6 uses its own threshold."""
        clients = [ets_client, ets_client2]
        try:
            for client in clients:
                await client.subscribe_and_wait_for_ack(_EVENTGROUP_5)
                await client.subscribe_and_wait_for_ack(_EVENTGROUP_6)
            await _burst(clients, METHOD_TRIGGER_EVENT_UINT8)
            for client in clients:
                assert client.received_counters(EVENT_UINT8) == (_COUNTERS, [])
            await _burst(clients)
            for client in clients:
                assert client.received_counters(EVENT_UINT8_MULTICAST) == eventgroup6.shared
        finally:
            await _unsubscribe_all(clients)
