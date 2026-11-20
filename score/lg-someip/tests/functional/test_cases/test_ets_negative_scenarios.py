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

"""SOME/IP negative tests that send crafted UDP frames to the DUT.

The tests send unknown service and method IDs, wrong message types and return codes, junk on the SD port,
SD messages with unreferenced options or trailing bytes, and concurrent requests with the same client ID.

Two negative scenarios are not in this file, because crafted UDP frames alone cannot cause them:

  - "Unreachable remote service" (E_NOT_REACHABLE): it needs a local ara::com proxy call to a spoofed remote
    instance.
  - "E2E failure injection for methods": the ETS configuration has no E2E-protected methods.
    Also, the method-call E2E check only logs a failure and continues.

TestSomeipLengthOverclaim checks an out-of-bounds read that can crash the daemon.
It is a separate class and it is the last class in this file.
Thus a crash cannot cause the tests of TestSomeipNegativeMisc to fail.
Its test is skipped and not xfailed, because xfail runs the body and can crash the daemon.
"""

from __future__ import annotations

import asyncio
import socket
import struct

import pytest
from attribute_plugin import add_test_properties
from scapy.contrib.automotive.someip import SDOption_Config
from scapy.packet import Raw
from someip_itf.ets.calls import assert_ok
from someip_itf.ets.client import (
    SD,
    SDENTRY_TYPE_EVTGRP_SUBSCRIBE,
    SDENTRY_TYPE_SRV_FINDSERVICE,
    SOMEIP,
    MessageType,
    ReturnCode,
    SDEntry_EventGroup,
    SDEntry_Service,
    SDOption_IP4_EndPoint,
)
from someip_itf.ets.constants import (
    ETS_INSTANCE_ID,
    ETS_MAJOR_VERSION,
    ETS_SERVER_UDP_PORT,
    ETS_SERVICE_ID,
    EVENTGROUP_UNICAST,
    FIELD_UINT8_INIT,
    METHOD_ECHO_UINT8,
    METHOD_FIELD_UINT8_GET,
    METHOD_FIELD_UINT8_SET,
    METHOD_RESET_INTERFACE,
    SD_MULTICAST_GROUP,
    SD_PORT,
    call,
    trigger,
)
from someip_itf.ets.raw import (
    ets_frame,
    expect_no_response,
    misdelivered_to,
    open_bound_socket,
    send_and_capture_subscribe_ack,
)

pytestmark = pytest.mark.asyncio(loop_scope="class")

_NO_RESPONSE_TIMEOUT = 3.0
# Different from FIELD_UINT8_INIT, so `test_request_to_fire_and_forget_method_gets_no_reply` can see if
# resetInterface ran.
_FIELD_UINT8_MARKER = 0x42
assert _FIELD_UINT8_MARKER != FIELD_UINT8_INIT
# Different from the ports that other test modules use.
_RAW_TEST_PORT = 60023
# SOME/IP-SD header flags with Reboot (0x80) and Unicast (0x40) set.
_SD_FLAGS_REBOOT_UNICAST = 0xC0

# The race in `test_concurrent_same_client_id_no_cross_delivery` is probabilistic.
# The server-side overlap depends on the scheduling jitter of the OS and container in the daemon-to-app IPC.
# Thus one attempt sometimes does not reproduce it (a strict XPASS).
# The test tries a limited number of times and accepts the first reproduction.
_RACE_ATTEMPTS = 10

# MESSAGE_SIZE_UNLIMITED is the only limit that the caller of GetMessageSize applies.
# The daemon gives all sizes up to this limit directly to Packet::CopyFrom.
_MESSAGE_SIZE_UNLIMITED = 65535
# The largest `length` field value that keeps msg_size (= 8 + length) at or below 65535.
_MAX_LENGTH_FIELD = _MESSAGE_SIZE_UNLIMITED - 8

# A service_id that the ETS configuration does not provide.
_UNKNOWN_SERVICE_ID = 0x9999


class TestSomeipNegativeMisc:
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_077", "SOMEIPSRV_ONWIRE_10"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_unknown_service_id_returns_e_unknown_service(self, ets_client, host_ip):
        """An unknown service_id gets ERROR/E_UNKNOWN_SERVICE.

        The header-only ERROR copies the request header.
        The interface version is different from ETS_MAJOR_VERSION, so the copy cannot match by accident.
        """
        frame = ets_frame(_UNKNOWN_SERVICE_ID, METHOD_ECHO_UINT8, b"\x42", iface_ver=0x07)

        sock = open_bound_socket(_RAW_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            data, _ = await asyncio.wait_for(loop.sock_recvfrom(sock, 65535), timeout=_NO_RESPONSE_TIMEOUT)
        finally:
            sock.close()

        assert data[:4] == frame[:4], "ERROR must copy the request's Message ID"
        assert data[8:12] == frame[8:12], "ERROR must copy the request's Request ID"
        assert data[12:14] == frame[12:14], "ERROR must copy the request's protocol and interface version"
        assert data[14] == int(MessageType.ERROR)
        assert data[15] == int(ReturnCode.E_UNKNOWN_SERVICE)
        assert len(data) == 16, f"ERROR must carry no payload, got {len(data)} bytes"

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # BUG: both clients use the wire client_id=0 (fixed in _send).
    # HandleRequest writes remote_clients_[client_id][service_instance].
    # It is a single-slot map with no session or port to tell the clients apart.
    # SendResponse can read it after the request of ets_client2 overwrote it.
    # Then the reply for ets_client goes to ets_client2.
    # ScapySomeipClient keeps such unmatched frames in _orphaned_responses.
    # Thus this test proves the misdelivery directly (misdelivered_to), not from a timeout only.
    # With no `await` between the sends, the first attempt reproduces it in each run.
    # strict=False: the spec requires a unique client_id for each client, so a shared one is possibly not a defect.
    # Make sure that the scenario is valid before this xfail is strict again.
    @pytest.mark.xfail(
        strict=False,
        reason=(
            "remote_clients_[client_id][service_instance] is a single-slot, "
            "last-writer-wins map with no session/port disambiguation; "
            "concurrent requests from two clients sharing wire client_id=0 "
            "can have their responses cross-delivered and silently dropped"
        ),
    )
    async def test_concurrent_same_client_id_no_cross_delivery(self, ets_client, ets_client2):
        loop = asyncio.get_running_loop()
        cross_delivery_detected = False

        for attempt in range(_RACE_ATTEMPTS):
            marker_a = bytes([0x01, attempt])
            marker_b = bytes([0x81, attempt])

            session_id_a = ets_client._next_session()
            fut_a: asyncio.Future = loop.create_future()
            ets_client._pending[session_id_a] = fut_a

            session_id_b = ets_client2._next_session()
            fut_b: asyncio.Future = loop.create_future()
            ets_client2._pending[session_id_b] = fut_b

            ets_client._orphaned_responses.clear()
            ets_client2._orphaned_responses.clear()

            # No `await` between these two sends -- this is the tightest gap
            # achievable from a single test process.
            ets_client._send(METHOD_ECHO_UINT8, marker_a, MessageType.REQUEST, session_id_a)
            ets_client2._send(METHOD_ECHO_UINT8, marker_b, MessageType.REQUEST, session_id_b)

            try:
                result_a, result_b = await asyncio.gather(
                    asyncio.wait_for(fut_a, timeout=_NO_RESPONSE_TIMEOUT),
                    asyncio.wait_for(fut_b, timeout=_NO_RESPONSE_TIMEOUT),
                )
            except asyncio.TimeoutError:
                # A bare timeout isn't proof -- only confirm cross-delivery
                # if the timed-out side's response shows up in the peer's
                # orphan list (session_id match). Check `.cancelled()`, not
                # `.done()`: `wait_for` cancels the timed-out future, and a
                # cancelled future is also "done" -- same as one that got a
                # real result -- so `.done()` can't tell them apart.
                # Otherwise this attempt is inconclusive: retry, and let the
                # server-alive probe below catch anything worse.
                if (fut_a.cancelled() and misdelivered_to(ets_client2, session_id_a)) or (
                    fut_b.cancelled() and misdelivered_to(ets_client, session_id_b)
                ):
                    cross_delivery_detected = True
                    break
                continue
            finally:
                ets_client._pending.pop(session_id_a, None)
                ets_client2._pending.pop(session_id_b, None)

            if result_a.payload[:1] != marker_a[:1] or result_b.payload[:1] != marker_b[:1]:
                cross_delivery_detected = True
                break

        assert not cross_delivery_detected, (
            "ets_client and/or ets_client2 received a response that does not "
            "match its own request, or the peer client captured a response "
            "misdelivered to it that was addressed to the other's session_id "
            "-- cross-client response delivery due to the shared wire "
            "client_id=0"
        )

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # Junk bytes on the SD port fail SDMessage::Deserialize().
    # On a nullptr result, the daemon returns early and does not process entries. It logs the error.
    # The test sends plain unicast to the container IP of the daemon.
    # A socket bound to INADDR_ANY:SD_PORT accepts unicast to its own address, also with no multicast membership.
    # Thus the test needs no IP_MULTICAST_IF or IP_ADD_MEMBERSHIP setup.
    # There is no wire response that shows the drop, so this test checks only that the daemon is alive.
    async def test_junk_on_sd_port_dropped(self, ets_client, host_ip):
        junk = b"\xde\xad\xbe\xef"

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.sendto(junk, (host_ip, SD_PORT))
        finally:
            sock.close()

        await asyncio.sleep(0.2)
        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # session_id=0 must fail ValidateSession(), and the daemon must drop the message silently.
    # A missing unicast OfferService reply proves this only if HandleRemoteFindService sends a reply
    # for a valid session_id. It does not: it is a TODO stub, and there is no offer also with session_id=1.
    # Thus a missing reply proves nothing about ValidateSession().
    # The test is skipped, because it passes for the wrong reason.
    # Enable it again when HandleRemoteFindService sends its reply (a libsomeip change).
    @pytest.mark.skip(
        reason=(
            "HandleRemoteFindService never sends a unicast OfferService "
            "reply for any session_id (it is a TODO stub) -- this test cannot "
            "tell 'ValidateSession() rejected session_id=0' from 'the reply "
            "path is not implemented', so it passes for the wrong reason. "
            "HandleRemoteFindService must send its reply (a libsomeip "
            "change) before this test is useful again."
        ),
    )
    async def test_sd_entry_with_zero_session_id_dropped(self, ets_client, host_ip):
        find_entry = SDEntry_Service(
            type=SDENTRY_TYPE_SRV_FINDSERVICE,
            srv_id=ETS_SERVICE_ID,
            inst_id=ETS_INSTANCE_ID,
            major_ver=ETS_MAJOR_VERSION,
            ttl=3,
            minor_ver=0xFFFFFFFF,
        )
        sd_pkt = SD(entry_array=[find_entry])
        hdr = SOMEIP(
            srv_id=0xFFFF,
            sub_id=0x8100,
            client_id=0,
            session_id=0,  # not valid for SOME/IP-SD; the daemon must drop the message
            proto_ver=0x01,
            iface_ver=0x01,
            msg_type=int(MessageType.NOTIFICATION),
            retcode=int(ReturnCode.E_OK),
        )
        frame = bytes(hdr / sd_pkt)

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.sendto(frame, (host_ip, SD_PORT))
        finally:
            sock.close()

        got_offer = await ets_client.wait_for_unicast_offer(timeout=_NO_RESPONSE_TIMEOUT)
        assert not got_offer, (
            "Expected no OfferService reply for a FindService entry with "
            "session_id=0 (not valid for SOME/IP-SD), but one arrived -- "
            "ValidateSession() must drop it before "
            "HandleRemoteFindService runs."
        )

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    @pytest.mark.xfail(
        strict=True,
        reason=(
            "someipd limitation, confirmed on the real DUT: "
            "HandleRemoteFindService never sends a unicast OfferService reply for any session_id "
            "(it is a TODO stub) -- a "
            "correct, valid unicast FindService gets no unicast reply."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_171"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_sd_171_unicast_findservice_gets_unicast_reply(self, ets_client, host_ip):
        """Several unicast FindService messages get a unicast OfferService reply."""
        find_entry = SDEntry_Service(
            type=SDENTRY_TYPE_SRV_FINDSERVICE,
            srv_id=ETS_SERVICE_ID,
            inst_id=ETS_INSTANCE_ID,
            major_ver=ETS_MAJOR_VERSION,
            ttl=3,
            minor_ver=0xFFFFFFFF,
        )
        sd_pkt = SD(flags=_SD_FLAGS_REBOOT_UNICAST, entry_array=[find_entry])

        offer_wait = asyncio.create_task(ets_client.wait_for_unicast_offer(timeout=_NO_RESPONSE_TIMEOUT))
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            for session_id in range(1, 6):
                hdr = SOMEIP(
                    srv_id=0xFFFF,
                    sub_id=0x8100,
                    client_id=0,
                    session_id=session_id,
                    proto_ver=0x01,
                    iface_ver=0x01,
                    msg_type=int(MessageType.NOTIFICATION),
                    retcode=int(ReturnCode.E_OK),
                )
                sock.sendto(bytes(hdr / sd_pkt), (host_ip, SD_PORT))
        finally:
            sock.close()

        got_offer = await offer_wait
        assert got_offer, (
            "expected a unicast OfferService reply to several unicast FindService messages, but no reply arrived"
        )

    @pytest.mark.xfail(
        strict=True,
        reason=(
            "someipd limitation, confirmed on the real DUT: "
            "HandleRemoteFindService never sends a unicast OfferService reply for any session_id -- "
            "a repeated multicast FindService does not help, because the reply path is "
            "not implemented for any FindService address."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_127"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_sd_127_multicast_findservice_gets_unicast_reply(self, ets_client, tester_ip):
        """A repeated multicast FindService gets a unicast OfferService reply."""
        find_entry = SDEntry_Service(
            type=SDENTRY_TYPE_SRV_FINDSERVICE,
            srv_id=ETS_SERVICE_ID,
            inst_id=ETS_INSTANCE_ID,
            major_ver=ETS_MAJOR_VERSION,
            ttl=3,
            minor_ver=0xFFFFFFFF,
        )
        sd_pkt = SD(flags=_SD_FLAGS_REBOOT_UNICAST, entry_array=[find_entry])

        async def _send_repeated_finds() -> None:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            # Make sure that the multicast FindService goes out through the tester interface,
            # and not through the interface that the routing table picks in the Docker setup.
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(tester_ip))
            try:
                for session_id in range(1, 11):
                    hdr = SOMEIP(
                        srv_id=0xFFFF,
                        sub_id=0x8100,
                        client_id=0,
                        session_id=session_id,
                        proto_ver=0x01,
                        iface_ver=0x01,
                        msg_type=int(MessageType.NOTIFICATION),
                        retcode=int(ReturnCode.E_OK),
                    )
                    sock.sendto(bytes(hdr / sd_pkt), (SD_MULTICAST_GROUP, SD_PORT))
                    await asyncio.sleep(0.1)
            finally:
                sock.close()

        offer_wait = asyncio.create_task(ets_client.wait_for_unicast_offer(timeout=_NO_RESPONSE_TIMEOUT))
        await _send_repeated_finds()
        got_offer = await offer_wait
        assert got_offer, "expected a unicast OfferService reply after repeated multicast FindService"

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_175"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_sd_175_unreferenced_option_is_ignored(self, ets_dut, host_ip, tester_ip):  # noqa: ARG002
        """A Subscribe gets an Ack when its Options Array also has an unreferenced Configuration Option.

        The entry references only its IPv4 Endpoint Option.
        """
        real_ep = SDOption_IP4_EndPoint(addr=tester_ip, l4_proto=0x11, port=60097)
        unreferenced_cfg = SDOption_Config(cfg_str=SDOption_Config.make_string({"unused": "1"}))
        entry = SDEntry_EventGroup(
            type=SDENTRY_TYPE_EVTGRP_SUBSCRIBE,
            srv_id=ETS_SERVICE_ID,
            inst_id=ETS_INSTANCE_ID,
            major_ver=ETS_MAJOR_VERSION,
            ttl=3,
            eventgroup_id=EVENTGROUP_UNICAST,
            n_opt_1=1,  # references only real_ep (index 0); no entry references unreferenced_cfg (index 1)
            index_1=0,
        )
        frame = bytes(
            SOMEIP(
                srv_id=0xFFFF,
                sub_id=0x8100,
                client_id=0,
                session_id=1,
                proto_ver=0x01,
                iface_ver=0x01,
                msg_type=int(MessageType.NOTIFICATION),
                retcode=int(ReturnCode.E_OK),
            )
            / SD(flags=_SD_FLAGS_REBOOT_UNICAST, entry_array=[entry], option_array=[real_ep, unreferenced_cfg])
        )

        ack = await send_and_capture_subscribe_ack(frame, host_ip, tester_ip)
        assert ack is not None, "no SubscribeEventgroupAck/Nack received"
        assert ack.ttl > 0, (
            f"expected a positive Ack, because the referenced option is valid; got ttl={ack.ttl} from the DUT"
        )

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_176"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_sd_176_unused_payload_after_options_array(self, ets_dut, host_ip, tester_ip):  # noqa: ARG002
        """A Subscribe with trailing bytes after its Options Array gets an Ack, in `length` or not.

        The trailing bytes are ``0x30303a3031``.
        The first Subscribe counts them in the outer SOME/IP ``length`` field, the second does not.
        """
        extra_payload = bytes.fromhex("30303a3031")
        ep = SDOption_IP4_EndPoint(addr=tester_ip, l4_proto=0x11, port=60096)

        def _entry() -> SDEntry_EventGroup:
            return SDEntry_EventGroup(
                type=SDENTRY_TYPE_EVTGRP_SUBSCRIBE,
                srv_id=ETS_SERVICE_ID,
                inst_id=ETS_INSTANCE_ID,
                major_ver=ETS_MAJOR_VERSION,
                ttl=3,
                eventgroup_id=EVENTGROUP_UNICAST,
                n_opt_1=1,
                index_1=0,
            )

        # Variant 1: the outer SOME/IP `length` field includes the extra payload.
        # The DUT must find the real end of the SD structure from its Entries and Options Array length fields.
        # It must not use all the bytes that the outer header gives.
        counted_frame = bytes(
            SOMEIP(
                srv_id=0xFFFF,
                sub_id=0x8100,
                client_id=0,
                session_id=1,
                proto_ver=0x01,
                iface_ver=0x01,
                msg_type=int(MessageType.NOTIFICATION),
                retcode=int(ReturnCode.E_OK),
            )
            / SD(flags=_SD_FLAGS_REBOOT_UNICAST, entry_array=[_entry()], option_array=[ep])
            / Raw(load=extra_payload)
        )
        ack_counted = await send_and_capture_subscribe_ack(counted_frame, host_ip, tester_ip)
        assert ack_counted is not None, "no Ack for the length-counted trailing payload variant"
        assert ack_counted.ttl > 0, (
            f"expected a positive Ack (length-counted variant); got ttl={ack_counted.ttl} from the DUT"
        )

        # Variant 2: the same trailing bytes, but the outer `length` field does not include them.
        uncounted_frame = (
            bytes(
                SOMEIP(
                    srv_id=0xFFFF,
                    sub_id=0x8100,
                    client_id=0,
                    session_id=2,
                    proto_ver=0x01,
                    iface_ver=0x01,
                    msg_type=int(MessageType.NOTIFICATION),
                    retcode=int(ReturnCode.E_OK),
                )
                / SD(flags=_SD_FLAGS_REBOOT_UNICAST, entry_array=[_entry()], option_array=[ep])
            )
            + extra_payload
        )
        ack_uncounted = await send_and_capture_subscribe_ack(uncounted_frame, host_ip, tester_ip)
        assert ack_uncounted is not None, "no Ack for the length-excluded trailing payload variant"
        assert ack_uncounted.ttl > 0, (
            f"expected a positive Ack (length-excluded variant); got ttl={ack_uncounted.ttl} from the DUT"
        )

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_177"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_sd_177_unused_payload_not_counted_in_length(self, ets_dut, host_ip, tester_ip):  # noqa: ARG002
        """A Subscribe with trailing bytes that the outer SOME/IP ``length`` does not include gets an Ack.

        The wire scenario is the same as the second variant of the previous test.
        The spec has two separate cases for it, so there are two separate tests.
        """
        extra_payload = bytes.fromhex("30303a3031")
        ep = SDOption_IP4_EndPoint(addr=tester_ip, l4_proto=0x11, port=60095)
        entry = SDEntry_EventGroup(
            type=SDENTRY_TYPE_EVTGRP_SUBSCRIBE,
            srv_id=ETS_SERVICE_ID,
            inst_id=ETS_INSTANCE_ID,
            major_ver=ETS_MAJOR_VERSION,
            ttl=3,
            eventgroup_id=EVENTGROUP_UNICAST,
            n_opt_1=1,
            index_1=0,
        )
        frame = (
            bytes(
                SOMEIP(
                    srv_id=0xFFFF,
                    sub_id=0x8100,
                    client_id=0,
                    session_id=1,
                    proto_ver=0x01,
                    iface_ver=0x01,
                    msg_type=int(MessageType.NOTIFICATION),
                    retcode=int(ReturnCode.E_OK),
                )
                / SD(flags=_SD_FLAGS_REBOOT_UNICAST, entry_array=[entry], option_array=[ep])
            )
            + extra_payload
        )

        ack = await send_and_capture_subscribe_ack(frame, host_ip, tester_ip)
        assert ack is not None, "no SubscribeEventgroupAck/Nack received"
        assert ack.ttl > 0, f"expected a positive Ack; got ttl={ack.ttl}"

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_140"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_subscribe_nonexistent_eventgroup_server_alive(self, ets_client):
        """A Subscribe for an unknown eventgroup gets a NACK (SUBSCRIBE_ACK with TTL 0).

        The DUT sends the NACK and does not ignore the entry.
        The SD monitor socket of ets_client captures the NACK, so the test checks the NACK and not only survival.
        """
        ets_client.subscribe_eventgroup(0xBEEF, 5)

        ttl = await ets_client.wait_for_subscribe_ack(0xBEEF, timeout=_NO_RESPONSE_TIMEOUT)
        assert ttl == 0, f"Expected a NACK (ttl=0) for a nonexistent eventgroup, got ttl={ttl}"

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

        ets_client.unsubscribe_eventgroup(0xBEEF)

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_075"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_garbage_message_type_dropped(self, ets_client, host_ip):
        """The DUT drops a frame with message type 0x55 and sends no reply.

        0x55 is not a defined message type.
        It goes to the final `else` of OnReceive, which logs an error.
        """
        frame = ets_frame(ETS_SERVICE_ID, METHOD_ECHO_UINT8, b"\x42", msg_type=0x55)

        sock = open_bound_socket(_RAW_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    @pytest.mark.parametrize("retcode", [0x20, 0xFF])
    @add_test_properties(
        partially_verifies=["SOMEIPSRV_RPC_07", "SOMEIPSRV_RPC_08"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_nonzero_return_code_on_request_ignored(self, ets_client, host_ip, retcode):
        """(variant): the DUT echoes a REQUEST with a return code above 0x1F.

        Variant: the OA case sends 0xC0 to test the two most significant bits. This test sends 0x20 and 0xFF.
        Only return codes in the error range 0x01-0x1F stop a REQUEST.
        """
        frame = ets_frame(ETS_SERVICE_ID, METHOD_ECHO_UINT8, b"\x42", retcode=retcode)

        sock = open_bound_socket(_RAW_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            data, _ = await asyncio.wait_for(loop.sock_recvfrom(sock, 65535), timeout=_NO_RESPONSE_TIMEOUT)
        finally:
            sock.close()

        assert data[14] == int(MessageType.RESPONSE)
        assert data[15] == int(ReturnCode.E_OK)
        assert data[16:17] == b"\x42"

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_076", "SOMEIPSRV_ONWIRE_12"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_unknown_method_id_returns_e_unknown_method(self, ets_client, host_ip):
        """An unmapped method_id on a known service gets ERROR/E_UNKNOWN_METHOD."""
        frame = ets_frame(ETS_SERVICE_ID, 0xFFFF, b"\x42")

        sock = open_bound_socket(_RAW_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            try:
                data, _ = await asyncio.wait_for(loop.sock_recvfrom(sock, 65535), timeout=_NO_RESPONSE_TIMEOUT)
            except asyncio.TimeoutError:
                pytest.fail("Expected ERROR/E_UNKNOWN_METHOD for an unmapped method_id, but the server sent no reply.")
        finally:
            sock.close()

        assert data[14] == int(MessageType.ERROR)
        assert data[15] == int(ReturnCode.E_UNKNOWN_METHOD)

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_076"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_unregistered_plausible_method_id_returns_e_unknown_method(self, ets_client, host_ip):
        """The unregistered method_id 7 gets ERROR/E_UNKNOWN_METHOD.

        ID 7 is between the ETS method IDs 1-4 and 8-65, so no special case for sentinel IDs can apply.
        """
        frame = ets_frame(ETS_SERVICE_ID, 7, b"\x42")

        sock = open_bound_socket(_RAW_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            try:
                data, _ = await asyncio.wait_for(loop.sock_recvfrom(sock, 65535), timeout=_NO_RESPONSE_TIMEOUT)
            except asyncio.TimeoutError:
                pytest.fail(
                    "Expected ERROR/E_UNKNOWN_METHOD for an unregistered method_id, but the server sent no reply."
                )
        finally:
            sock.close()

        assert data[14] == int(MessageType.ERROR)
        assert data[15] == int(ReturnCode.E_UNKNOWN_METHOD)

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    @add_test_properties(
        partially_verifies=["SOMEIPSRV_RPC_10"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_request_to_fire_and_forget_method_gets_no_reply(self, ets_client, host_ip):
        """A REQUEST to the fire-and-forget method resetInterface gets no reply.

        The DUT must not send an ERROR for a fire-and-forget method with Message Type Request.
        The DUT discards the message, so fieldUINT8 keeps its earlier value and does not go to FIELD_UINT8_INIT.
        """
        set_result = await call(ets_client, METHOD_FIELD_UINT8_SET, bytes([_FIELD_UINT8_MARKER]))
        assert set_result.return_code == ReturnCode.E_OK

        frame = ets_frame(
            ETS_SERVICE_ID,
            METHOD_RESET_INTERFACE,
            b"",
            msg_type=int(MessageType.REQUEST),
        )

        sock = open_bound_socket(_RAW_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        try:
            get_result = await call(ets_client, METHOD_FIELD_UINT8_GET, b"")
            assert_ok(get_result)
            assert get_result.payload[0] == _FIELD_UINT8_MARKER, "resetInterface ran for a discarded REQUEST"
        finally:
            await trigger(ets_client, METHOD_RESET_INTERFACE, b"")

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_075"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_notification_with_method_id_gets_no_reply(self, ets_client, host_ip):
        """A NOTIFICATION to echoUINT8 gets no reply.

        NOTIFICATION is a valid message type, but it is not correct for a method endpoint.
        The DUT must not send an ERROR for notifications, so it drops the message with no reply.
        """
        frame = ets_frame(
            ETS_SERVICE_ID,
            METHOD_ECHO_UINT8,
            b"\x42",
            msg_type=int(MessageType.NOTIFICATION),
        )

        sock = open_bound_socket(_RAW_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)


class TestSomeipLengthOverclaim:
    """A separate class on purpose, as the module docstring tells.

    UDPServerEndpoint::Receive() reads into a fixed 1416-byte thread_local buffer (kMaxUDPMessageSize).
    HandleUdpIncomingData uses the header `length` field (Utility::GetMessageSize).
    It checks the field only against the 65535 limit, not against the real size of the buffer.
    Packet::CopyFrom then copies up to that size with memcpy.
    For a 16-byte datagram, this is an out-of-bounds read of up to ~63KB (CWE-125).
    The crash is confirmed but probabilistic: it occurred in 2 of 3 runs.
    """

    # BUG (skip, confirmed but probabilistic: it crashed in 2 of 3 runs).
    # In a run with a crash, the 16-byte frame of this test is the last "UDP received" line in the container log.
    # No RX or dispatch line follows, but a normal follow-up call gives one.
    # This must be skip and not xfail: xfail runs the body.
    # A crash stops the container, and then all subsequent tests fail.
    # The skip lets the other tests in this file run in default runs.
    @pytest.mark.skip(
        reason=(
            "one-packet remote crash: a 16-byte datagram with a too large "
            "`length` header field makes Packet::CopyFrom "
            "copy up to 65535 bytes from a 1416-byte thread_local "
            "receive buffer, an out-of-bounds "
            "read (CWE-125) that crashes the daemon"
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_058"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_oversized_length_field_does_not_crash_daemon(self, ets_client, host_ip):
        """(partial): a 16-byte REQUEST with a too large `length` does not stop the daemon.

        Partial: the test sends one request, not several as the case does, and checks only that the daemon is alive.
        The header gives the 65535-byte message limit, but the datagram has no payload.
        """
        # A minimal 16-byte SOME/IP header. The test sends no payload.
        # The `length` field (bytes 4-7) gives an incorrect size of the data that follows.
        frame = struct.pack(
            ">HHIHHBBBB",
            ETS_SERVICE_ID,
            METHOD_ECHO_UINT8,
            _MAX_LENGTH_FIELD,
            0x0001,
            0x0001,
            0x01,
            ETS_MAJOR_VERSION,
            int(MessageType.REQUEST),
            0x00,
        )
        assert len(frame) == 16

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
        finally:
            sock.close()

        # Give the daemon time to process or discard the frame, or to crash.
        await asyncio.sleep(1.0)

        try:
            probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        except asyncio.TimeoutError:
            pytest.fail(
                "Daemon did not respond after it received a datagram with a "
                "too large `length` field -- this agrees with a crash caused "
                "by the out-of-bounds read in Packet::CopyFrom "
                f"(msg_size up to {_MESSAGE_SIZE_UNLIMITED} bytes copied from "
                "a 1416-byte receive buffer)."
            )

        assert probe.message_type == MessageType.RESPONSE
        assert probe.return_code == ReturnCode.E_OK
        assert probe.payload[:1] == b"\x42"
