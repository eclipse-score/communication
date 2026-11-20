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

"""Service availability tests for the ETS SOME/IP service.

The tests use suspendInterface(start: u8, duration: u8). This is method ID 2,
fire-and-forget, over UDP. The payload is bytes([start, duration]).
The server sends no response, so a TimeoutError is expected.

The server waits start seconds and calls StopOfferService(). Then it waits
duration seconds and calls OfferService().
"""

from __future__ import annotations

import asyncio
import time

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.calls import fire_suspend, get_field_uint8, wait_for_availability
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import (
    ETS_MAJOR_VERSION,
    ETS_SERVER_UDP_PORT,
    EVENT_UINT8_E2E,
    FIELD_UINT8_INIT,
    METHOD_FIELD_UINT8_GET,
    METHOD_FIELD_UINT8_SET,
    call,
)
from someip_itf.ets.raw import udp_endpoint_port

pytestmark = pytest.mark.asyncio(loop_scope="class")


#: Differs from FIELD_UINT8_INIT, so a field reset to its initial value during suspend is observable.
_FIELD_UINT8_SET_VALUE = 0xA7


class TestEtsServiceAvailability:
    @add_test_properties(
        partially_verifies=["AS__SWS_CM_00111", "AS__SWS_CM_00101", "SOMEIP_ETS_163"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_suspend_interface_toggles_availability(self, ets_client):
        """suspendInterface takes the service offline and then offers it again.

        The test does not check the Getter, the Setter or the StopOfferService.
        Method calls show the availability.
        """
        assert await ets_client.is_available(), "pre-condition: service must be offered"

        # suspendInterface(start=1, duration=4): ETS calls StopOfferService() after
        # approximately 1 s. It calls OfferService() approximately 4 s later.
        #
        # The test cannot check the timing precisely. The client detects "not offered"
        # when the TTL expires (offer TTL is 3 s, cyclic offers at 1 Hz).
        # The last offer can be up to 1 s older than StopOfferService.
        # Thus the down edge can occur up to 3 s after StopOfferService.
        # The gap between the down edge and the up edge is approximately 1-2 s, not duration.
        # The test checks only that both transitions occur.
        await fire_suspend(ets_client, 1, 4)

        await wait_for_availability(ets_client, False, timeout=6.0)
        await wait_for_availability(ets_client, True, timeout=8.0)

    # StopOffer deletes the server-side subscription. No events arrive after the re-offer until the client subscribes again.
    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00428"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_stop_offer_deletes_subscription(self, ets_client, ets_eventgroup):
        """After a StopOffer and re-offer cycle, the server sends no EVENT_UINT8_E2E until the client subscribes again."""
        timestamps: list[float] = []

        def _cb(eid: int, _payload: bytes) -> None:
            if eid == EVENT_UINT8_E2E:
                timestamps.append(time.monotonic())

        ets_client.register_callback(_cb)
        # The 30 s TTL is longer than the test. Only a deletion on the server can end this subscription.
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        try:
            # Baseline: make sure that the 1 Hz publisher sends events.
            await asyncio.sleep(2.5)
            assert len(timestamps) >= 1, (
                "expected ≥1 EVENT_UINT8_E2E sample at 1 Hz over 2.5 s baseline; "
                "check that the service offers TestEventUINT8E2E continuously"
            )

            # Now only the server can end the subscription.
            ets_client.stop_maintaining_subscription(ets_eventgroup)

            t_suspend = time.monotonic()
            await fire_suspend(ets_client, 0, 4)
            await wait_for_availability(ets_client, False, timeout=3.0)
            await wait_for_availability(ets_client, True, timeout=8.0)
            t_up = time.monotonic()

            # If the subscription is still active, approximately 3 samples at 1 Hz arrive in this window.
            await asyncio.sleep(3.0)
            after_reoffer = [ts for ts in timestamps if ts >= t_up]
            assert not after_reoffer, (
                f"{len(after_reoffer)} event(s) received after re-offer without resubscribing, "
                f"first at t_up+{after_reoffer[0] - t_up:.2f} s; StopOffer must delete the server-side subscription"
            )

            # Positive control: the publisher runs again. Thus the deleted subscription caused the silence above.
            t_resubscribe = time.monotonic()
            ets_client.subscribe_eventgroup(ets_eventgroup, 30)
            await asyncio.sleep(2.5)
            assert any(ts >= t_resubscribe for ts in timestamps), (
                "expected ≥1 EVENT_UINT8_E2E within 2.5 s of resubscribing after re-offer"
            )
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)
            await asyncio.sleep(0.3)

        # Allow 0.5 s at the start for the StopOffer to take effect.
        in_window = [ts for ts in timestamps if t_suspend + 0.5 <= ts < t_up]
        assert not in_window, (
            f"{len(in_window)} event(s) received while the service was stopped; "
            f"first at t+{in_window[0] - t_suspend:.2f} s"
        )

    # The server NACKs a Subscribe for a stopped service. It ACKs the Subscribe sent after the re-offer.
    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00393", "AS__PRS_SOMEIPSD_00566", "AS__PRS_SOMEIPSD_00462"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_subscribe_during_suspension_window(self, ets_client, ets_eventgroup):
        """The server NACKs a Subscribe while the service is stopped and ACKs the one the client sends on re-offer."""
        assert await ets_client.is_available(), "pre-condition: service must be offered"

        # Stop the service immediately (start=0) for 4 s.
        await fire_suspend(ets_client, 0, 4)
        await wait_for_availability(ets_client, False, timeout=3.0)

        samples: list[bytes] = []

        def _cb(eid: int, payload: bytes) -> None:
            if eid == EVENT_UINT8_E2E:
                samples.append(payload)

        ets_client.register_callback(_cb)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        try:
            nack_ttl = await ets_client.wait_for_subscribe_ack(ets_eventgroup, timeout=2.0)
            assert nack_ttl == 0, f"expected a NACK (ttl=0) for a Subscribe to a stopped service, got ttl={nack_ttl}"

            # The client sends the Subscribe again when it sees the re-offer.
            # Start the wait first, so that the test does not miss the ACK.
            ack_task = asyncio.ensure_future(ets_client.wait_for_subscribe_ack(ets_eventgroup, timeout=8.0))
            await wait_for_availability(ets_client, True, timeout=6.0)
            ack_ttl = await ack_task
            assert ack_ttl > 0, f"expected an ACK (ttl>0) for the Subscribe sent after re-offer, got ttl={ack_ttl}"

            await asyncio.sleep(3.0)
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)
            await asyncio.sleep(0.3)

        assert len(samples) >= 1, "expected ≥1 EVENT_UINT8_E2E after the ACKed re-subscription"

    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00356",
            "AS__PRS_SOMEIPSD_00357",
            "AS__PRS_SOMEIPSD_00363",
            "AS__PRS_SOMEIPSD_00364",
            "AS__PRS_SOMEIPSD_00840",
            "AS__SWS_CM_00203",
            "AS__SWS_CM_00204",
            "SOMEIP_ETS_089",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_same_port_answers_calls_before_and_after_suspend(self, ets_client):
        """The port that answers a Getter before a suspendInterface cycle also answers it after the cycle.

        The test does not call the Setter or TestFieldUINT8Array. All calls use one tester source port.
        The Offer, StopOffer and re-Offer have the same UDP port. The StopOffer has the versions of the Offer.
        """
        assert await ets_client.is_available(), "pre-condition: service must be offered"

        before = await call(ets_client, METHOD_FIELD_UINT8_GET, b"")
        assert before.message_type == MessageType.RESPONSE
        assert before.return_code == ReturnCode.E_OK

        # Start the capture before the suspend, because the server sends the StopOffer only once.
        ets_client.start_offer_capture()
        try:
            offer = await ets_client.next_offer_entry(stop=False, timeout=5.0)
            await fire_suspend(ets_client, 0, 3)
            stop_offer = await ets_client.next_offer_entry(stop=True, timeout=5.0)
            reoffer = await ets_client.next_offer_entry(stop=False, timeout=8.0)
        finally:
            ets_client.stop_offer_capture()

        assert offer.major_version == ETS_MAJOR_VERSION, f"Offer major version {offer.major_version}"
        assert udp_endpoint_port(offer) == ETS_SERVER_UDP_PORT, (
            f"Offer names UDP port {udp_endpoint_port(offer)}, expected {ETS_SERVER_UDP_PORT}"
        )

        # next_offer_entry already filters type, service ID and instance ID. Thus only the versions are compared.
        offer_versions = (offer.major_version, offer.service_minor_version)
        stop_offer_versions = (stop_offer.major_version, stop_offer.service_minor_version)
        assert stop_offer_versions == offer_versions, (
            f"StopOffer (major, minor) {stop_offer_versions} differs from the Offer's {offer_versions}"
        )
        assert udp_endpoint_port(stop_offer) == udp_endpoint_port(offer), (
            f"StopOffer names UDP port {udp_endpoint_port(stop_offer)}, the Offer it stops {udp_endpoint_port(offer)}"
        )

        assert udp_endpoint_port(reoffer) == udp_endpoint_port(offer), (
            f"re-Offer names UDP port {udp_endpoint_port(reoffer)}, the original Offer {udp_endpoint_port(offer)}"
        )

        await wait_for_availability(ets_client, True, timeout=6.0)

        after = await asyncio.wait_for(
            call(ets_client, METHOD_FIELD_UINT8_GET, b""),
            timeout=5.0,
        )
        assert after.message_type == MessageType.RESPONSE, (
            f"no RESPONSE from {ETS_SERVER_UDP_PORT} after suspend/resume; "
            "the port that answered before the suspension must also answer after it"
        )
        assert after.return_code == ReturnCode.E_OK

    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIP_00181", "AS__PRS_SOMEIP_00182", "SOMEIP_ETS_163"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_field_state_survives_suspend(self, ets_client):
        """A TestFieldUINT8 value set before a suspendInterface cycle is the same after the cycle.

        The test does not check the StopOfferService.
        The OA v3.0 specification is not consistent here. Its synopsis says that the interface does not reset.
        Its step 8 requires a value different from the value set. This test follows the synopsis.
        """
        assert await ets_client.is_available(), "pre-condition: service must be offered"

        initial = await get_field_uint8(ets_client)
        assert initial != _FIELD_UINT8_SET_VALUE, (
            f"pre-condition: TestFieldUINT8 already holds 0x{_FIELD_UINT8_SET_VALUE:02x}, so keeping it proves nothing"
        )

        try:
            set_result = await call(ets_client, METHOD_FIELD_UINT8_SET, bytes([_FIELD_UINT8_SET_VALUE]))
            assert set_result.message_type == MessageType.RESPONSE
            assert set_result.return_code == ReturnCode.E_OK
            assert set_result.payload == bytes([_FIELD_UINT8_SET_VALUE]), (
                f"setter response payload {set_result.payload.hex()} does not carry the value set"
            )

            await fire_suspend(ets_client, 0, 3)
            await wait_for_availability(ets_client, False, timeout=3.0)
            await wait_for_availability(ets_client, True, timeout=6.0)

            after = await get_field_uint8(ets_client)
            assert after == _FIELD_UINT8_SET_VALUE, (
                f"TestFieldUINT8 was 0x{_FIELD_UINT8_SET_VALUE:02x} before suspend, got 0x{after:02x} "
                f"after resume (initial value 0x{FIELD_UINT8_INIT:02x}); suspendInterface must not reset field state"
            )
        finally:
            await call(ets_client, METHOD_FIELD_UINT8_SET, bytes([FIELD_UINT8_INIT]))
