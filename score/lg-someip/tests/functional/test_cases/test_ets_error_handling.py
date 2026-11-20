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

"""Error handling tests for the ETS SOME/IP service.

The tests send requests with header errors, wrong versions, unknown method IDs
and truncated payloads. The test client can see these results:

    TimeoutError    - the stack dropped the request and sent no reply
    E_OK response   - the server processed the request although the header is not correct
    ERROR response  - the server rejected the request with the specified return code

The server rejects a truncated payload with ERROR/E_MALFORMED_MESSAGE.
After each test that expects no reply, a valid call makes sure that the server
did not crash or lock.
"""

from __future__ import annotations

import asyncio
import socket

import pytest
import pytest_asyncio
from attribute_plugin import add_test_properties
from someip_itf.ets.calls import assert_ok
from someip_itf.ets.client import MessageType, ReturnCode, ScapySomeipClient
from someip_itf.ets.constants import (
    ETS_INSTANCE_ID,
    ETS_SERVER_UDP_PORT,
    ETS_SERVICE_ID,
    EVENT_UINT8,
    METHOD_ECHO_UINT8,
    METHOD_FIELD_UINT8_GET,
    METHOD_FIELD_UINT8_SET,
    METHOD_TRIGGER_EVENT_UINT8,
    call,
    trigger,
)
from someip_itf.ets.raw import ets_frame

pytestmark = pytest.mark.asyncio(loop_scope="class")

# A response from the server usually takes much less than 1 s.
# After 3 s, the test is sure that the server sent no reply.
_NO_RESPONSE_TIMEOUT = 3.0

# These UDP ports are different from CLIENT_ENDPOINT_PORT (60010) and
# CLIENT_ENDPOINT_PORT2 (60011) of the shared ets_client fixtures.
_BAD_CLIENT_PORT = 60020  # wrong-version ScapySomeipClient fixture
_RAW_TEST_PORT = 60021  # raw-socket tests that also receive replies


@pytest_asyncio.fixture(scope="class", loop_scope="class")
async def wrong_version_client(ets_dut, tester_ip, host_ip):
    """ScapySomeipClient with iface_ver=0xFF that does not use SD discovery.

    Tests use it to send correct SOME/IP frames with a wrong interface_version.
    The fixture gives the server IP and port directly, so no SD Find/Offer exchange is necessary.
    """
    client = ScapySomeipClient(
        service_id=ETS_SERVICE_ID,
        instance_id=ETS_INSTANCE_ID,
        client_ip=tester_ip,
        client_port=_BAD_CLIENT_PORT,
        major_version=0xFF,
        server_ip=host_ip,
        server_port=ETS_SERVER_UDP_PORT,
    )
    client.start(asyncio.get_running_loop())
    yield client
    client.close()


class TestSomeipNegative:
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_076", "SOMEIPSRV_ONWIRE_12"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_unknown_method_id_returns_e_unknown_method(self, ets_client):
        """An unknown method ID gets ERROR/E_UNKNOWN_METHOD.

        The ERROR has only a header.
        """
        result = await asyncio.wait_for(
            ets_client.call_method(0xFFFF, b"\x42"),
            timeout=_NO_RESPONSE_TIMEOUT,
        )
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_UNKNOWN_METHOD
        assert not result.payload, "an ERROR has no payload"

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_074"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_wrong_interface_version_echo_method_returns_error(self, wrong_version_client):
        """A wrong interface version on echoUINT8 gets ERROR/E_WRONG_INTERFACE_VERSION.

        The test case specification also accepts an ignored request. This test requires the ERROR.
        """
        result = await asyncio.wait_for(
            wrong_version_client.call_method(METHOD_ECHO_UINT8, b"\x42"),
            timeout=_NO_RESPONSE_TIMEOUT,
        )
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_WRONG_INTERFACE_VERSION
        assert not result.payload, "an ERROR has no payload"

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_074"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_wrong_interface_version_field_getter_returns_error(self, ets_client, wrong_version_client):
        """A wrong interface version on a getter gets E_WRONG_INTERFACE_VERSION.

        The request goes to the TestFieldUINT8 getter. The OA test case uses echoUINT8.
        """
        result = await asyncio.wait_for(
            wrong_version_client.call_method(METHOD_FIELD_UINT8_GET, b""),
            timeout=_NO_RESPONSE_TIMEOUT,
        )
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_WRONG_INTERFACE_VERSION
        assert not result.payload, "an ERROR has no payload"

    # The server rejects a zero-byte payload for echoUINT8 with E_MALFORMED_MESSAGE.
    # Unmarshaller::IsValid() finds the truncated payload. UnmarshalAndCall then
    # replies ERROR/E_MALFORMED_MESSAGE and does not call the method handler.
    async def test_zero_byte_payload_echo_uint8_malformed_message(self, ets_client):
        result = await asyncio.wait_for(
            ets_client.call_method(METHOD_ECHO_UINT8, b""),
            timeout=_NO_RESPONSE_TIMEOUT,
        )
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_075"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_response_type_message_to_server_ignored(self, ets_client, host_ip):
        """The server still answers after the test sends a RESPONSE to its port.

        The test does not capture the reply to the RESPONSE. Thus it does not check for an ERROR or no reply.
        OnReceive sends it to HandleResponse. HandleResponse finds no related pending request in the daemon.
        """
        frame = ets_frame(
            ETS_SERVICE_ID,
            METHOD_ECHO_UINT8,
            b"\x42",
            msg_type=int(MessageType.RESPONSE),
        )
        raw = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            raw.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
        finally:
            raw.close()

        await asyncio.sleep(0.2)
        result = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(result)

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_078", "SOMEIPSRV_ONWIRE_05"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_wrong_protocol_version_echo_method_returns_error(self, ets_client, host_ip):
        """A wrong protocol version gets ERROR/E_WRONG_PROTOCOL_VERSION."""
        loop = asyncio.get_running_loop()
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("", _RAW_TEST_PORT))
        sock.setblocking(False)

        frame = ets_frame(
            ETS_SERVICE_ID,
            METHOD_ECHO_UINT8,
            b"\x42",
            proto_ver=0xFF,
            session_id=0x0099,
        )
        try:
            await loop.sock_sendto(sock, frame, (host_ip, ETS_SERVER_UDP_PORT))
            data, _ = await asyncio.wait_for(
                loop.sock_recvfrom(sock, 65535),
                timeout=_NO_RESPONSE_TIMEOUT,
            )
        except asyncio.TimeoutError:
            pytest.fail("Expected E_WRONG_PROTOCOL_VERSION response for wrong proto_ver, but the server sent no reply.")
        finally:
            sock.close()

        assert len(data) >= 16, f"Response too short: {len(data)} bytes"
        msg_type = data[14]
        retcode = data[15]
        assert msg_type == int(MessageType.ERROR), (
            f"Expected ERROR (0x{int(MessageType.ERROR):02x}), got 0x{msg_type:02x}"
        )
        assert retcode == int(ReturnCode.E_WRONG_PROTOCOL_VERSION), (
            f"Expected E_WRONG_PROTOCOL_VERSION (0x{int(ReturnCode.E_WRONG_PROTOCOL_VERSION):02x}), got 0x{retcode:02x}"
        )

        result = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(result)

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_075"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_request_no_return_for_echo_method_no_response(self, ets_client, host_ip):
        """A REQUEST_NO_RETURN to echoUINT8 gets no reply.

        The message type check fails. The server sends no ERROR for a REQUEST_NO_RETURN.
        """
        loop = asyncio.get_running_loop()
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("", _RAW_TEST_PORT))
        sock.setblocking(False)

        frame = ets_frame(
            ETS_SERVICE_ID,
            METHOD_ECHO_UINT8,
            b"\x42",
            msg_type=int(MessageType.REQUEST_NO_RETURN),
            session_id=0x0098,
        )
        timed_out = False
        try:
            await loop.sock_sendto(sock, frame, (host_ip, ETS_SERVER_UDP_PORT))
            try:
                await asyncio.wait_for(
                    loop.sock_recvfrom(sock, 65535),
                    timeout=_NO_RESPONSE_TIMEOUT,
                )
            except asyncio.TimeoutError:
                timed_out = True
        finally:
            sock.close()

        assert timed_out, "Expected no reply to REQUEST_NO_RETURN, but server sent one"

        result = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(result)

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_059"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_wrong_interface_version_fire_and_forget_not_processed(
        self, ets_client, ets_eventgroup, wrong_version_client
    ):
        """A fire-and-forget call with a wrong interface version does not run.

        The call is triggerEventUINT8, not resetInterface. The test does not check for an ERROR reply.
        A missing TestEventUINT8 shows that the server discarded the call.
        First, a trigger with the correct version shows that the subscription works.
        """
        # Maximum wait time for the start of a triggered TestEventUINT8 burst.
        event_timeout = 15.0
        # After this time without a notification, a burst is finished and a discarded trigger is silent.
        event_quiet_period = 3.0
        loop = asyncio.get_running_loop()
        received: list[float] = []

        def _on_event(event_id: int, _payload: bytes) -> None:
            if event_id == EVENT_UINT8:
                received.append(loop.time())

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)
        try:
            await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8, bytes([0, 3, 1]))
            deadline = loop.time() + event_timeout
            while not received:
                if loop.time() > deadline:
                    pytest.fail(f"control trigger produced no TestEventUINT8 within {event_timeout:.0f} s")
                await asyncio.sleep(0.1)
            while loop.time() - received[-1] < event_quiet_period:
                await asyncio.sleep(0.1)

            received.clear()
            wrong_version_client.send_no_return(METHOD_TRIGGER_EVENT_UINT8, bytes([0, 3, 1]))
            await asyncio.sleep(event_quiet_period)
            assert not received, (
                f"{len(received)} TestEventUINT8 notification(s) after a triggerEventUINT8 with interface version "
                "0xFF; the call must be discarded"
            )
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_078"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_malformed_burst_server_survives(self, ets_client, host_ip):
        """The server still answers after 20 frames with protocol version 0xFF.

        The test sends a burst, not a single request. It does not check the reply (ERROR or no reply) to each frame.
        """
        frame = ets_frame(
            ETS_SERVICE_ID,
            METHOD_ECHO_UINT8,
            b"\x42",
            proto_ver=0xFF,
        )
        raw = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            for _ in range(20):
                raw.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
        finally:
            raw.close()

        await asyncio.sleep(0.5)
        result = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(result)
        assert result.payload[:1] == b"\x42"

    # The server rejects a zero-byte payload for the fieldUINT8 setter with
    # E_MALFORMED_MESSAGE. The set handler does not run.
    # MutableFieldImpl::handleSet checks Unmarshaller::IsValid() before it calls the handler.
    async def test_zero_byte_payload_field_uint8_set_malformed_message(self, ets_client):
        result = await asyncio.wait_for(
            ets_client.call_method(METHOD_FIELD_UINT8_SET, b""),
            timeout=_NO_RESPONSE_TIMEOUT,
        )
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)
