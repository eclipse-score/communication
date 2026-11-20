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

"""ETS client mode tests, against an ETS instance that the tester offers, as OPEN Alliance TC8 v3.0 specifies.

clientServiceActivate (47) makes the DUT find ETS ``0x0103``/``0x00F4``. ``TesterEts`` offers it from the tester.
clientServiceSubscribeEventgroup (50) subscribes the client of the DUT to eventgroups 5 and 6 of that instance.
The GetLastValue methods (60, 61) return the value that the tester sent there.
The DUT takes the delay and duration of clientServiceSubscribeEventgroup as one byte each. The OA spec uses Uint32.
Each case activates the client. The ``client_mode`` fixture deactivates it and stops the offer of the tester.
"""

from __future__ import annotations

import asyncio
from typing import NoReturn

import pytest
import pytest_asyncio
from attribute_plugin import add_test_properties
from someip_itf.codec import SdEntry, SdEntryType
from someip_itf.ets.calls import assert_ok, call_echo, get_last_value_multicast, get_last_value_unicast
from someip_itf.ets.client_mode import SD_TIMEOUT, ClientModeSession, assert_echo_requests, is_eventgroup
from someip_itf.ets.constants import (
    ETS_MAJOR_VERSION,
    EVENT_UINT8,
    EVENT_UINT8_MULTICAST,
    EVENTGROUP_MULTICAST,
    EVENTGROUP_UNICAST,
    METHOD_CLIENT_SERVICE_DEACTIVATE,
    METHOD_CLIENT_SERVICE_SUBSCRIBE_EVENTGROUP,
    METHOD_ECHO_UINT8,
    METHOD_RESET_INTERFACE,
    TESTER_ETS_MULTICAST_GROUP,
    TESTER_ETS_REBOOT_UDP_PORT,
    TESTER_ETS_UDP_PORT,
    call,
    trigger,
)
from someip_itf.ets.payloads import unpack_array
from someip_itf.ets.provider import TesterEts
from someip_itf.ets.raw import udp_endpoint

_LGSOMEIP_1_XFAIL_REASON = (
    "[lg-someip #1] the daemon hands a remote server's Response to the app with instance 0x0000, so it reaches "
    "the app's own server handler instead of the client"
)
_LGSOMEIP_2_XFAIL_REASON = (
    "[lg-someip #2] a StopOffer after release_service marks the service as requested again, so the next "
    "request_service sends no FindService"
)

pytestmark = pytest.mark.asyncio(loop_scope="class")

_STOP_SUBSCRIBE_TIMEOUT = 3.0
# Longer than the latest Subscribe that SD_TIMEOUT allows, plus _STOP_SUBSCRIBE_TIMEOUT.
# Thus the end of the subscription cannot cause the StopSubscribe that the deactivate test expects.
_DEACTIVATE_SUBSCRIBE_DURATION = 20
_REBOOT_SUBSCRIBE_DURATION = 40  # longer than the 2 subscribe, deliver and echo rounds of the reboot test
_RESET_SETTLE = 0.1  # resetInterface is fire-and-forget
_DEACTIVATE_SETTLE = 1.0  # clientServiceDeactivate is fire-and-forget
_FIND_QUIET = 3.0  # three of the tester's cyclic offers, each for an instance the DUT already holds
_EVENT_VALUE = 0x08  # the value that OA TC8 sends to the client of the DUT

_ETS_082_XFAIL_REASON = (
    "after a remote reboot someipd can subscribe again before it closes the old UDP endpoint of the client; "
    "it logs 'UDP client: bind failed ... port: 34491', and in some runs the client then loses its route to the server"
)
_ETS_082_SYMPTOM = "UDP client: bind failed"


@pytest_asyncio.fixture(loop_scope="class")
async def client_mode(ets_client, tester_ip, host_ip):
    """A ``ClientModeSession``. Its tester ETS has open sockets, but does not offer the service yet.

    With no offer, a case can see the FindService of the DUT before the DUT knows the instance.
    resetInterface sets the GetLastValue caches to zero, so no case reads a value from an earlier case.
    """
    await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
    await asyncio.sleep(_RESET_SETTLE)
    session = ClientModeSession(ets_client, TesterEts(tester_ip, host_ip, TESTER_ETS_UDP_PORT))
    try:
        yield session
    finally:
        await session.close()


def _subscriber(entry: SdEntry) -> tuple[str, int]:
    endpoint = udp_endpoint(entry)
    if endpoint is None:
        pytest.fail(f"Subscribe for eventgroup {entry.eventgroup_id} references no UDP endpoint option")
    return endpoint


def _assert_old_port_unused(tester: TesterEts) -> None:
    stale = tester.retired_traffic()
    assert not stale, f"the DUT sent {len(stale)} datagrams to the old UDP port of the tester ETS"


def _xfail_if_bind_failed(ets_dut, tester: TesterEts, log_mark: int, failure: str) -> NoReturn:
    """Xfail if someipd logged its bind failure after *log_mark*. If not, fail with *failure*.

    Traffic to the pre-reboot port of the tester fails the case, also when someipd logged the bind failure.
    """
    _assert_old_port_unused(tester)
    if _ETS_082_SYMPTOM in ets_dut.someipd_process.get_output()[log_mark:]:
        pytest.xfail(_ETS_082_XFAIL_REASON)
    raise AssertionError(f"{failure}, and someipd logged no '{_ETS_082_SYMPTOM}'")


async def _assert_server_answers(ets_client) -> None:
    result = await call(ets_client, METHOD_ECHO_UINT8, bytes([0x42]))
    assert_ok(result)
    assert result.payload[:1] == bytes([0x42]), "server mode stopped its answers while the client mode was in operation"


class TestClientMode:
    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00350",
            "AS__PRS_SOMEIPSD_00351",
            "AS__PRS_SOMEIPSD_00361",
            "SOMEIP_ETS_099",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_1_XFAIL_REASON)
    async def test_ets_099_client_start_up_keeps_server_mode(self, client_mode):
        """Client mode starts with a FindService and operates while the server mode continues to answer.

        The echo goes to the endpoint in the offer of the tester.
        When the client has that offer, it sends no more FindService.
        """
        await _assert_server_answers(client_mode.ets_client)
        await client_mode.activate_and_find()
        client_mode.tester.start()

        await client_mode.echo_through_tester([0x10, 0x20])
        client_mode.tester.drain()
        finds = await client_mode.tester.collect(SdEntryType.FindService, _FIND_QUIET)
        assert not finds, f"the DUT sent {len(finds)} FindService for an instance that it already has an offer for"
        await _assert_server_answers(client_mode.ets_client)

    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00385",
            "AS__PRS_SOMEIPSD_00386",
            "AS__PRS_SOMEIPSD_00807",
            "SOMEIP_ETS_106",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    # Not strict: run without an earlier case that released the service, the case passes.
    @pytest.mark.xfail(strict=False, reason=_LGSOMEIP_2_XFAIL_REASON)
    async def test_ets_106_subscribe_after_offer(self, client_mode, host_ip):
        """(variant): the client subscribes to the ETS of the tester when the tester offers it.

        Variant: the delay and duration of clientServiceSubscribeEventgroup are one byte each, not Uint32.
        """
        subscribe = await client_mode.subscribe(EVENTGROUP_UNICAST)

        assert subscribe.major_version == ETS_MAJOR_VERSION, (
            f"Subscribe names major version {subscribe.major_version}, not the offered {ETS_MAJOR_VERSION}"
        )
        address, _port = _subscriber(subscribe)
        assert address == host_ip, f"Subscribe names {address} as its endpoint, not the DUT address {host_ip}"

    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00360", "AS__PRS_SOMEIPSD_00807", "SOMEIP_ETS_105"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_105_last_value_udp_unicast(self, client_mode):
        """(variant): GetLastValueOfEventUDPUnicast returns the event sent to the endpoint of the Subscribe.

        Variant: the delay and duration of clientServiceSubscribeEventgroup are one byte each, not Uint32.
        """
        subscribe = await client_mode.subscribe(EVENTGROUP_UNICAST)
        client_mode.tester.ack(subscribe)

        endpoint = _subscriber(subscribe)
        result = await client_mode.deliver(
            lambda: client_mode.tester.send_event(EVENT_UINT8, bytes([_EVENT_VALUE]), endpoint),
            get_last_value_unicast,
            _EVENT_VALUE,
        )
        assert_ok(result)
        assert result.payload[:1] == bytes([_EVENT_VALUE]), f"GetLastValue returned {list(result.payload)}"

    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00323", "SOMEIP_ETS_104"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    # Not strict: run without an earlier case that released the service, the case passes.
    @pytest.mark.xfail(strict=False, reason=_LGSOMEIP_2_XFAIL_REASON)
    async def test_ets_104_last_value_udp_multicast(self, client_mode):
        """(variant): GetLastValueOfEventUDPMulticast returns the event sent to the group in the Ack.

        Variant: the delay and duration of clientServiceSubscribeEventgroup are one byte each, not Uint32.
        """
        subscribe = await client_mode.subscribe(EVENTGROUP_MULTICAST)
        client_mode.tester.ack(subscribe, multicast_group=TESTER_ETS_MULTICAST_GROUP)

        result = await client_mode.deliver(
            lambda: client_mode.tester.send_event(
                EVENT_UINT8_MULTICAST, bytes([_EVENT_VALUE]), TESTER_ETS_MULTICAST_GROUP
            ),
            get_last_value_multicast,
            _EVENT_VALUE,
        )
        assert_ok(result)
        assert result.payload[:1] == bytes([_EVENT_VALUE]), f"GetLastValue returned {list(result.payload)}"

    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00388",
            "AS__PRS_SOMEIPSD_00389",
            "AS__PRS_SOMEIPSD_00431",
            "SOMEIP_ETS_084",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_084_deactivate_stops_subscription(self, client_mode):
        """(variant): clientServiceDeactivate makes the client send a StopSubscribe for its subscription.

        Variant: the delay and duration of clientServiceSubscribeEventgroup are one byte each, not Uint32.
        """
        subscribe = await client_mode.subscribe(
            EVENTGROUP_UNICAST, duration=_DEACTIVATE_SUBSCRIBE_DURATION, activate_delay=1
        )
        client_mode.tester.ack(subscribe)

        await trigger(client_mode.ets_client, METHOD_CLIENT_SERVICE_DEACTIVATE, bytes([0]))
        stop = await client_mode.tester.wait_for(
            SdEntryType.Subscribe,
            _STOP_SUBSCRIBE_TIMEOUT,
            lambda entry: entry.ttl == 0 and entry.eventgroup_id == EVENTGROUP_UNICAST,
        )
        assert stop is not None, f"no StopSubscribe for eventgroup {EVENTGROUP_UNICAST} after clientServiceDeactivate"
        assert (stop.major_version, stop.minver_or_counter) == (subscribe.major_version, subscribe.minver_or_counter), (
            "StopSubscribe major version and counter are different from the Subscribe that it stops"
        )
        assert udp_endpoint(stop) == udp_endpoint(subscribe), (
            f"StopSubscribe names endpoint {udp_endpoint(stop)}, the Subscribe {udp_endpoint(subscribe)}"
        )

    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00449", "SOMEIP_ETS_098"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    # Not strict: run without an earlier case that released the service, the case passes.
    @pytest.mark.xfail(strict=False, reason=_LGSOMEIP_2_XFAIL_REASON)
    async def test_ets_098_no_subscribe_without_trigger(self, client_mode):
        """Offers alone do not make the client subscribe without clientServiceSubscribeEventgroup.

        The window has five offers of the tester.
        The echo after the window shows that the client found the ETS of the tester.
        """
        await client_mode.activate_and_find()
        client_mode.tester.start()

        subscribes = await client_mode.tester.collect(SdEntryType.Subscribe, 5.0, lambda entry: entry.ttl > 0)
        assert not subscribes, f"the DUT subscribed eventgroups {[e.eventgroup_id for e in subscribes]} untriggered"
        await client_mode.echo_through_tester([0x5A])

    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00351", "AS__PRS_SOMEIPSD_00430", "SOMEIP_ETS_101"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_101_stop_offer_stops_find(self, client_mode):
        """A StopOffer that answers the first FindService of the client stops its FindService repetitions.

        The count starts at the StopOffer, because the repetition phase ends in less than one second.
        One FindService that is already in transit when the StopOffer goes out is permitted.
        """
        await client_mode.activate_and_find()
        client_mode.tester.stop_offer()
        client_mode.tester.drain()

        finds = await client_mode.tester.collect(SdEntryType.FindService, 4.0)
        assert len(finds) <= 1, f"the DUT sent {len(finds)} more FindService for 0x0103/0x00F4 after the StopOffer"

    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00385", "AS__PRS_SOMEIPSD_00449", "SOMEIP_ETS_082"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_1_XFAIL_REASON)
    async def test_ets_082_server_reboot_new_udp_port(self, client_mode, ets_dut):
        """(variant): after the ETS of the tester reboots onto a new UDP port, the client subscribes again.

        Variant: the delay and duration of clientServiceSubscribeEventgroup are one byte each, not Uint32.
        After the reboot, only the new port answers. Any datagram from the DUT to the old port fails the case.
        The test also checks event delivery after the reboot, which the pass criterion of the case does not require.
        A cyclic offer alone also causes a new Subscribe.
        The echo on the new port shows that the client uses the new endpoint.
        A lost route after the reboot is an empty echo or a call that times out.
        It is an expected failure only when someipd logged the bind failure.
        """
        tester = client_mode.tester
        subscribe = await client_mode.subscribe(EVENTGROUP_UNICAST, duration=_REBOOT_SUBSCRIBE_DURATION)
        tester.ack(subscribe)
        endpoint = _subscriber(subscribe)
        before = await client_mode.deliver(
            lambda: tester.send_event(EVENT_UINT8, bytes([_EVENT_VALUE]), endpoint),
            get_last_value_unicast,
            _EVENT_VALUE,
        )
        assert_ok(before)
        assert before.payload[:1] == bytes([_EVENT_VALUE]), (
            f"before the reboot GetLastValue read {list(before.payload)}"
        )

        log_mark = len(ets_dut.someipd_process.get_output())
        await asyncio.to_thread(tester.reboot, TESTER_ETS_REBOOT_UDP_PORT)
        resubscribe = await tester.wait_for(SdEntryType.Subscribe, SD_TIMEOUT, is_eventgroup(EVENTGROUP_UNICAST))
        assert resubscribe is not None, "the DUT did not subscribe again after the tester's ETS rebooted"
        tester.ack(resubscribe)
        try:
            echo = await call_echo(client_mode.ets_client, [0x09])
        except asyncio.TimeoutError:
            _xfail_if_bind_failed(ets_dut, tester, log_mark, "the echo after the reboot timed out")
        _assert_old_port_unused(tester)
        assert_ok(echo)
        if unpack_array(echo.payload) == [] and not tester.requests:
            _xfail_if_bind_failed(ets_dut, tester, log_mark, "the echo after the reboot did not reach the tester ETS")
        assert unpack_array(echo.payload) == [0x09], f"client echo returned {unpack_array(echo.payload)}"
        assert_echo_requests(tester.requests, [[0x09]])

        endpoint = _subscriber(resubscribe)
        try:
            after = await client_mode.deliver(
                lambda: tester.send_event(EVENT_UINT8, bytes([_EVENT_VALUE + 1]), endpoint),
                get_last_value_unicast,
                _EVENT_VALUE + 1,
            )
        except asyncio.TimeoutError:
            _xfail_if_bind_failed(ets_dut, tester, log_mark, "GetLastValue after the reboot timed out")
        _assert_old_port_unused(tester)
        assert_ok(after)
        if after.payload[:1] != bytes([_EVENT_VALUE + 1]):
            _xfail_if_bind_failed(
                ets_dut, tester, log_mark, f"after the reboot GetLastValue read {list(after.payload)}"
            )

    # Not strict: run without an earlier case that released the service, the case passes.
    @pytest.mark.xfail(strict=False, reason=_LGSOMEIP_2_XFAIL_REASON)
    async def test_concurrent_echo(self, client_mode):
        """Two clientServiceCallEchoUINT8Array calls at the same time each return their own array."""
        await client_mode.activate_and_find()
        client_mode.tester.start()

        results = await asyncio.gather(
            call_echo(client_mode.ets_client, [0x11]),
            call_echo(client_mode.ets_client, [0x22]),
            return_exceptions=True,
        )
        for result in results:
            assert not isinstance(result, Exception), f"echo call raised: {result!r}"
            assert_ok(result)
        payloads = {tuple(unpack_array(result.payload)) for result in results}
        assert payloads == {(0x11,), (0x22,)}, f"expected [0x11] and [0x22], got {payloads}"
        assert_echo_requests(client_mode.tester.requests, [[0x11], [0x22]])

    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_1_XFAIL_REASON)
    async def test_echo_after_deactivate_returns_empty(self, client_mode):
        """After clientServiceDeactivate, clientServiceCallEchoUINT8Array returns an empty array with no remote call.

        The ETS of the tester continues to offer and answer, so an empty reply shows that the DUT dropped its proxy.
        """
        await client_mode.activate_and_find()
        client_mode.tester.start()
        await client_mode.echo_through_tester([0x33])

        await trigger(client_mode.ets_client, METHOD_CLIENT_SERVICE_DEACTIVATE, bytes([0]))
        await asyncio.sleep(_DEACTIVATE_SETTLE)
        answered = len(client_mode.tester.requests)

        result = await call_echo(client_mode.ets_client, [0xAA])
        assert_ok(result)
        assert unpack_array(result.payload) == [], f"expected an empty array, got {unpack_array(result.payload)}"
        assert not client_mode.tester.requests[answered:], "the DUT client called the tester ETS after deactivation"

    # Skip and not xfail: xfail runs the body, and the 100 subscribe cycles take ~450 s
    # before the unconditional fail below.
    @pytest.mark.skip(reason="requires container-level /proc/<pid>/fd introspection")
    async def test_fd_leak(self, ets_client):
        for _ in range(100):
            await trigger(
                ets_client,
                METHOD_CLIENT_SERVICE_SUBSCRIBE_EVENTGROUP,
                bytes([0, 2]),
            )
            await asyncio.sleep(2.5)
        # No assertion. Pass or fail needs an fd count comparison in the container.
        pytest.fail("fd count verification not implemented")
