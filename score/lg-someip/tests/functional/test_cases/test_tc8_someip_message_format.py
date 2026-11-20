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
"""TC8 SOME/IP message format tests.

The tests send SOME/IP requests over UDP and TCP to someipd and check the replies.
They check the response header fields, the error return codes and the processing of malformed messages.
They also check fire-and-forget requests, unaligned messages in one segment, and the byte order of the header.
"""

import functools
import socket
import time

import pytest
from scapy.contrib.automotive.someip import SDOption_IP4_EndPoint
from attribute_plugin import add_test_properties
from someip_itf.codec import L4Proto, MessageType, ReturnCode, SomeipResponse, parse_someip
from someip_itf.tc8 import message_builder
from someip_itf.tc8.asserts import (
    assert_client_echo,
    assert_offer_has_tcp_endpoint_option,
    assert_return_code,
    assert_session_echo,
    assert_valid_response,
)
from someip_itf.tc8.constants import (
    DUT_RELIABLE_PORT,
    DUT_UNRELIABLE_PORT,
    MAJOR_VERSION,
    METHOD_ID,
    SERVICE_ID,
)
from someip_itf.tc8.dut_lifecycle import TargetStack
from someip_itf.tc8.message_builder import build_truncated_message, build_zero_length_message
from someip_itf.tc8.probes import (
    assert_dut_alive,
    send_request_and_receive,
    send_request_expect_no_response,
    wait_for_dut_offer,
)
from someip_itf.tc8.sd import capture_sd_offers
from someip_itf.tc8.udp import udp_receive_responses, udp_send_concatenated
from someip_itf.tcp import (
    tcp_connect,
    tcp_receive_n_responses,
    tcp_receive_response,
    tcp_send_concatenated,
    tcp_send_request,
)

# ---------------------------------------------------------------------------
# Module-level configuration
# ---------------------------------------------------------------------------

_UNKNOWN_METHOD_ID: int = 0xBEEF

# Requests carry the deployed interface version unless a test passes interface_version on purpose.
_build_notification_as_request = functools.partial(
    message_builder.build_notification_as_request, interface_version=MAJOR_VERSION
)
_build_oversized_message = functools.partial(message_builder.build_oversized_message, interface_version=MAJOR_VERSION)
_build_request = functools.partial(message_builder.build_request, interface_version=MAJOR_VERSION)
_build_request_no_return = functools.partial(message_builder.build_request_no_return, interface_version=MAJOR_VERSION)
_build_request_with_return_code = functools.partial(
    message_builder.build_request_with_return_code, interface_version=MAJOR_VERSION
)
_build_wrong_protocol_version_request = functools.partial(
    message_builder.build_wrong_protocol_version_request, interface_version=MAJOR_VERSION
)


# ---------------------------------------------------------------------------
# Response header
# ---------------------------------------------------------------------------


class TestSomeipResponseHeader:
    """Tests for the response header fields."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header", "AS__PRS_SOMEIP_00052"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_001_protocol_version(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """RESPONSE has protocol_version = 0x01."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0001)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert resp.protocol_version == 1, f"protocol_version = {resp.protocol_version}, expected 1 (0x01)"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00055", "AS__PRS_SOMEIP_00922"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_002_message_type_response(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Response to REQUEST has message_type = RESPONSE (0x80)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0002)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert_valid_response(resp, SERVICE_ID, METHOD_ID)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00055"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_002_no_response_for_request_no_return(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Fire-and-forget: the DUT must not send a RESPONSE to a REQUEST_NO_RETURN."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request_no_return(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0009)
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        assert not responses, (
            f"{len(responses)} response(s) received for REQUEST_NO_RETURN; "
            "the SOME/IP specification requires no response to fire-and-forget messages"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00922", "AS__PRS_SOMEIP_00704"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_005_session_id_echo(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """RESPONSE session_id is equal to REQUEST session_id."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        session_id = 0x1234
        req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=session_id)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert_session_echo(resp, session_id)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00922", "AS__PRS_SOMEIP_00704"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_008_client_id_echo(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """RESPONSE client_id is equal to REQUEST client_id."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        client_id = 0x0011
        req = _build_request(SERVICE_ID, METHOD_ID, client_id=client_id, session_id=0x0003)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert_client_echo(resp, client_id)


# ---------------------------------------------------------------------------
# Error return codes
# ---------------------------------------------------------------------------


class TestSomeipErrorCodes:
    """Tests for the error return codes."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_error_codes"],
        partially_verifies=["AS__PRS_SOMEIP_00055", "AS__PRS_SOMEIP_00191", "SOMEIPSRV_ONWIRE_10"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_003_unknown_service(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Request to unknown service gets an ERROR / E_UNKNOWN_SERVICE reply.

        The DUT must reply to a REQUEST for a service that it does not offer.
        The reply has message_type ERROR and return_code E_UNKNOWN_SERVICE (0x02).
        A silent drop is not compliant.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        unknown_service = 0xBEEF
        req = _build_request(unknown_service, METHOD_ID, client_id=0x0010, session_id=0x0004)
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        assert responses, (
            f"no response received for unknown service_id 0x{unknown_service:04x}; "
            "an ERROR / E_UNKNOWN_SERVICE reply is required"
        )
        assert responses[0].message_type == MessageType.ERROR, (
            f"message_type mismatch: got 0x{responses[0].message_type:02x}, expected ERROR (0x{MessageType.ERROR:02x})"
        )
        assert_return_code(responses[0], ReturnCode.E_UNKNOWN_SERVICE)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_error_codes"],
        partially_verifies=["AS__PRS_SOMEIP_00191"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_004_unknown_method(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Request with unknown method_id gets E_UNKNOWN_METHOD."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, _UNKNOWN_METHOD_ID, client_id=0x0010, session_id=0x0005)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert_return_code(resp, ReturnCode.E_UNKNOWN_METHOD)

    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIP_00190", "AS__PRS_SOMEIP_00191", "SOMEIP_ETS_077"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_077_wrong_service_id_error_or_ignore(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Request to unknown service gets ERROR/E_UNKNOWN_SERVICE, or the DUT ignores it.

        The OA pass criteria also accept no reply, thus this test accepts no reply.
        test_tc8_msg_003_unknown_service requires the ERROR reply, and the DUT sends it.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        unknown_service = 0xBEEF
        req = _build_request(unknown_service, METHOD_ID, client_id=0x0011, session_id=0x0004)
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        if responses:
            assert responses[0].message_type == MessageType.ERROR, (
                f"message_type mismatch: got 0x{responses[0].message_type:02x}, "
                f"expected ERROR (0x{MessageType.ERROR:02x})"
            )
            assert_return_code(responses[0], ReturnCode.E_UNKNOWN_SERVICE)
        else:
            assert_dut_alive(tc8_dut_ip, session_id=0x0104)

    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIP_00058", "AS__PRS_SOMEIP_00190", "AS__PRS_SOMEIP_00191", "SOMEIP_ETS_076"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_076_wrong_method_id_error_or_ignore(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Request with unknown method_id gets E_UNKNOWN_METHOD, or the DUT ignores it.

        Partial: the test checks only the return code of an ERROR reply, not its message_type.
        `test_tc8_msg_004_unknown_method` always requires E_UNKNOWN_METHOD.
        The OA pass criteria also accept no reply, thus this test accepts no reply.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, _UNKNOWN_METHOD_ID, client_id=0x0011, session_id=0x0005)
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        if responses:
            assert_return_code(responses[0], ReturnCode.E_UNKNOWN_METHOD)
        else:
            assert_dut_alive(tc8_dut_ip, session_id=0x0105)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_error_codes"],
        partially_verifies=["AS__PRS_SOMEIP_00190", "AS__PRS_SOMEIP_00191"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_006_wrong_interface_version(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Request with wrong interface version must get E_WRONG_INTERFACE_VERSION.

        The interface_version of the REQUEST is not equal to the version of the offered service.
        The server must send E_WRONG_INTERFACE_VERSION (0x08). vsomeip 3.6.1 sends this return code.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(
            SERVICE_ID,
            METHOD_ID,
            client_id=0x0010,
            session_id=0x0006,
            interface_version=0xFF,
        )
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert resp.message_type == MessageType.ERROR, (
            f"message_type mismatch: got 0x{resp.message_type:02x}, expected ERROR (0x{MessageType.ERROR:02x})"
        )
        assert_return_code(resp, ReturnCode.E_WRONG_INTERFACE_VERSION)
        assert resp.interface_version == 0xFF, (
            f"ERROR must copy the request's interface_version, got 0x{resp.interface_version:02x}"
        )


# ---------------------------------------------------------------------------
# Malformed messages
# ---------------------------------------------------------------------------


class TestMalformedMessages:
    """The DUT must not crash when it receives malformed SOME/IP messages."""

    @pytest.mark.xfail(
        strict=True,
        reason=(
            "someipd limitation, seen on the real DUT: the daemon drops a message under 8 bytes and sends no "
            "ERROR/E_MALFORMED_MESSAGE response. The cause is the same as in test_ets_054_empty_payload_request: "
            "the daemon does not compare the length field with the mandatory 8-byte header tail before it "
            "discards a message that it cannot parse."
        ),
    )
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_malformed"],
        partially_verifies=["AS__PRS_SOMEIP_00910", "AS__PRS_SOMEIP_00191", "SOMEIP_ETS_055"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_007_truncated_message_no_crash(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Variant: a message under 8 bytes gets ERROR/E_MALFORMED_MESSAGE, and the DUT does not crash.

        Variant: the message is a 7-byte datagram, not a full header with a Length field below 8.
        The OA test requires the ERROR reply. A silent drop is a failure.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        responses = send_request_expect_no_response(tc8_dut_ip, build_truncated_message(), timeout_secs=2.0)
        resp = responses[0] if responses else None

        assert someipd_dut.poll() is None, "someipd crashed after receiving a truncated SOME/IP message"

        assert resp is not None, (
            "no response to a message under 8 bytes; expected an ERROR reply with return code E_MALFORMED_MESSAGE"
        )
        is_malformed_error = (
            resp.message_type == MessageType.ERROR and resp.return_code == ReturnCode.E_MALFORMED_MESSAGE
        )
        assert is_malformed_error, (
            f"response to a message under 8 bytes was "
            f"message_type=0x{resp.message_type:02x} return_code=0x{resp.return_code:02x}; "
            "expected ERROR/E_MALFORMED_MESSAGE"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_malformed"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_007_wrong_protocol_version_no_crash(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Message with protocol_version=0xFF does not crash the DUT."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        malformed = _build_wrong_protocol_version_request(
            SERVICE_ID,
            METHOD_ID,
            client_id=0x0010,
            session_id=0x0007,
        )
        # The test checks only that the DUT is alive. It ignores all replies.
        send_request_expect_no_response(tc8_dut_ip, malformed, timeout_secs=1.0)

        assert someipd_dut.poll() is None, (
            "someipd crashed after it received a REQUEST with the wrong protocol version (protocol_version=0xFF)"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_malformed"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_msg_007_oversized_length_field_no_crash(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Message with a length field of 0x7FF3 bytes does not crash the DUT."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.sendto(
                _build_oversized_message(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0008),
                (tc8_dut_ip, DUT_UNRELIABLE_PORT),
            )
        finally:
            sock.close()

        time.sleep(0.3)
        assert someipd_dut.poll() is None, (
            "someipd crashed after it received a message with an oversized length field (0x7FF3 bytes)"
        )


# ---------------------------------------------------------------------------
# TCP transport binding
# ---------------------------------------------------------------------------


class TestSomeipTcpTransport:
    """Tests for the TCP transport binding.

    The tests make sure that someipd processes SOME/IP request/response correctly over TCP.
    The DUT has an unreliable port (UDP ``DUT_UNRELIABLE_PORT``) and a reliable port (TCP ``DUT_RELIABLE_PORT``).
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__tcp_transport"],
        partially_verifies=["AS__PRS_SOMEIP_00707", "AS__PRS_SOMEIP_00940", "SOMEIPSRV_RPC_01"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_rpc_01_tcp_request_response(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """REQUEST over TCP gets a valid RESPONSE.

        The DUT accepts a TCP connection and sends a RESPONSE with message_type=0x80 to a SOME/IP REQUEST.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        sock = tcp_connect(tc8_dut_ip, DUT_RELIABLE_PORT)
        try:
            req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0050)
            tcp_send_request(sock, req)
            resp = tcp_receive_response(sock)

            assert_valid_response(resp, SERVICE_ID, METHOD_ID)
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__tcp_transport"],
        partially_verifies=["AS__PRS_SOMEIP_00922", "AS__PRS_SOMEIP_00704", "SOMEIPSRV_RPC_01"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_rpc_01_tcp_session_id_echo(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """TCP RESPONSE has the same session_id as the REQUEST."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        session_id = 0x5678
        sock = tcp_connect(tc8_dut_ip, DUT_RELIABLE_PORT)
        try:
            req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=session_id)
            tcp_send_request(sock, req)
            resp = tcp_receive_response(sock)

            assert_session_echo(resp, session_id)
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__tcp_transport"],
        partially_verifies=["AS__PRS_SOMEIP_00922", "AS__PRS_SOMEIP_00704", "SOMEIPSRV_RPC_01"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_rpc_01_tcp_client_id_echo(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """TCP RESPONSE has the same client_id as the REQUEST."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        client_id = 0x0015
        sock = tcp_connect(tc8_dut_ip, DUT_RELIABLE_PORT)
        try:
            req = _build_request(SERVICE_ID, METHOD_ID, client_id=client_id, session_id=0x0051)
            tcp_send_request(sock, req)
            resp = tcp_receive_response(sock)

            assert_client_echo(resp, client_id)
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__tcp_transport"],
        partially_verifies=["AS__PRS_SOMEIP_00707", "AS__PRS_SOMEIP_00940", "SOMEIPSRV_RPC_01", "SOMEIPSRV_RPC_02"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_rpc_02_tcp_multiple_methods_single_connection(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Two method Request/Response calls over one shared TCP connection.

        This test is a variant of the "single TCP connection for methods" check.
        It does not do the full OA check, which sends all Notifications over one TCP connection.
        The TCP event notification tests do the full check.
        They are xfail, because TCP events do not operate on this DUT.

        The DUT must process the request/response exchanges on the same TCP connection without a new connection.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        sock = tcp_connect(tc8_dut_ip, DUT_RELIABLE_PORT)
        try:
            req1 = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0060)
            tcp_send_request(sock, req1)
            resp1 = tcp_receive_response(sock)
            assert_valid_response(resp1, SERVICE_ID, METHOD_ID)
            assert_session_echo(resp1, 0x0060)

            req2 = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0061)
            tcp_send_request(sock, req2)
            resp2 = tcp_receive_response(sock)
            assert_valid_response(resp2, SERVICE_ID, METHOD_ID)
            assert_session_echo(resp2, 0x0061)
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__tcp_transport"],
        partially_verifies=["AS__PRS_SOMEIPSD_00307", "AS__PRS_SOMEIPSD_00310", "SOMEIPSRV_OPTIONS_15"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_options_15_tcp_endpoint_advertised(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """SD OfferService includes a TCP endpoint option.

        The DUT has a reliable (TCP) port. Thus the SD OfferService must include an IPv4EndpointOption
        with L4Proto=TCP and the correct port.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        offers = capture_sd_offers(tc8_host_ip, min_count=1, timeout_secs=5.0)
        assert offers, "No SD OfferService received"

        assert_offer_has_tcp_endpoint_option(offers[0], tc8_dut_ip, DUT_RELIABLE_PORT)


# ---------------------------------------------------------------------------
# Unaligned messages over TCP
# ---------------------------------------------------------------------------


class TestTcpUnalignedMessages:
    """Tests for unaligned SOME/IP messages in one TCP segment.

    A SOME/IP TCP receiver must use only the length field to find the message limits in the byte stream.
    Consecutive messages are not necessarily aligned to 4 bytes.
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__tcp_transport"],
        partially_verifies=["AS__PRS_SOMEIP_00140", "AS__PRS_SOMEIP_00142", "SOMEIP_ETS_068"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_ets_068_unaligned_someip_messages_over_tcp(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Three REQUEST messages in one TCP segment. The third message is unaligned.

        Payload layout in the concatenated TCP write:
          msg1 (session 0x0071): 0-byte payload  -> 16 bytes (offset   0, aligned)
          msg2 (session 0x0072): 2-byte payload  -> 18 bytes (offset  16, aligned)
          msg3 (session 0x0073): 0-byte payload  -> 16 bytes (offset  34, NOT 4-byte aligned)

        The DUT must send a separate RESPONSE to each of the three messages.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        msg1 = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0071, payload=b"")
        msg2 = _build_request(
            SERVICE_ID,
            METHOD_ID,
            client_id=0x0010,
            session_id=0x0072,
            payload=b"\xaa\xbb",
        )
        msg3 = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0073, payload=b"")

        sock = tcp_connect(tc8_dut_ip, DUT_RELIABLE_PORT)
        try:
            tcp_send_concatenated(sock, [msg1, msg2, msg3])
            responses = tcp_receive_n_responses(sock, count=3, timeout_secs=5.0)
        finally:
            sock.close()

        assert len(responses) == 3, f"expected 3 RESPONSE messages, got {len(responses)}"
        for resp in responses:
            assert resp.service_id == SERVICE_ID
            assert_valid_response(resp, SERVICE_ID, METHOD_ID)

        session_ids = {resp.session_id for resp in responses}
        assert session_ids == {0x0071, 0x0072, 0x0073}, (
            f"unexpected session IDs in responses: {{{', '.join(f'0x{s:04x}' for s in sorted(session_ids))}}}"
        )


# ---------------------------------------------------------------------------
# Unaligned messages over UDP
# ---------------------------------------------------------------------------


class TestUdpUnalignedMessages:
    """Tests for unaligned SOME/IP messages in one UDP datagram.

    A SOME/IP UDP receiver must use only the length field to find each message in the datagram.
    Consecutive messages are not necessarily aligned to 4 bytes.
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__udp_transport"],
        partially_verifies=["AS__PRS_SOMEIP_00140", "AS__PRS_SOMEIP_00142", "SOMEIP_ETS_069"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_ets_069_unaligned_someip_messages_over_udp(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Three REQUEST messages in one UDP datagram. The third message is unaligned.

        Payload layout in the concatenated UDP write:
          msg1 (session 0x0081): 0-byte payload  -> 16 bytes (offset  0, aligned)
          msg2 (session 0x0082): 2-byte payload  -> 18 bytes (offset 16, aligned)
          msg3 (session 0x0083): 0-byte payload  -> 16 bytes (offset 34, NOT 4-byte aligned)

        The DUT must send a separate RESPONSE to each of the three messages.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        msg1 = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0081, payload=b"")
        msg2 = _build_request(
            SERVICE_ID,
            METHOD_ID,
            client_id=0x0010,
            session_id=0x0082,
            payload=b"\xaa\xbb",
        )
        msg3 = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0010, session_id=0x0083, payload=b"")

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("", 0))
        try:
            udp_send_concatenated(sock, (tc8_dut_ip, DUT_UNRELIABLE_PORT), [msg1, msg2, msg3])
            responses = udp_receive_responses(sock, count=3, timeout_secs=5.0)
        finally:
            sock.close()

        assert len(responses) == 3, f"expected 3 RESPONSE messages, got {len(responses)}"
        for resp in responses:
            assert resp.service_id == SERVICE_ID
            assert_valid_response(resp, SERVICE_ID, METHOD_ID)

        session_ids = {resp.session_id for resp in responses}
        assert session_ids == {0x0081, 0x0082, 0x0083}, (
            f"unexpected session IDs in responses: {{{', '.join(f'0x{s:04x}' for s in sorted(session_ids))}}}"
        )


# ---------------------------------------------------------------------------
# Service identification
# ---------------------------------------------------------------------------


_UNKNOWN_SERVICE_ID: int = 0xBEEF
_EVENT_METHOD_ID: int = 0x8001  # Bit 15 set: event notification


def _send_request_and_receive_with_addr(
    endpoint: tuple[str, int],
    request_bytes: bytes,
    timeout_secs: float = 3.0,
) -> tuple[SomeipResponse, tuple[str, int]]:
    """Send a SOME/IP request to *endpoint* and return (response, (src_ip, src_port))."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("", 0))
        sock.sendto(request_bytes, endpoint)
        sock.settimeout(timeout_secs)
        data, addr = sock.recvfrom(65535)
        resp = parse_someip(data)
        return resp, addr
    finally:
        sock.close()


class TestSomeipBasicIdentifiers:
    """Tests for service identification."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00922", "SOMEIPSRV_BASIC_01"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_basic_01_correct_service_id_gets_response(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """REQUEST to a known service and method gets a RESPONSE with E_OK."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0020, session_id=0x0001)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert_valid_response(resp, SERVICE_ID, METHOD_ID)
        assert_return_code(resp, ReturnCode.E_OK)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_error_codes"],
        partially_verifies=["AS__PRS_SOMEIP_00191", "SOMEIPSRV_ONWIRE_10"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_basic_02_unknown_service_id_no_response_or_error(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """REQUEST to unknown service_id must get E_UNKNOWN_SERVICE.

        The test name refers to the reserved SD instance ID check, which is a different requirement.
        The DUT must reply to a REQUEST for an unknown service_id with E_UNKNOWN_SERVICE (return_code 0x02).
        test_tc8_msg_003_unknown_service does the same check.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(_UNKNOWN_SERVICE_ID, METHOD_ID, client_id=0x0020, session_id=0x0002)
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        assert responses, (
            "No response received for unknown service_id "
            f"0x{_UNKNOWN_SERVICE_ID:04x}; DUT must reply with E_UNKNOWN_SERVICE"
        )
        assert_return_code(responses[0], ReturnCode.E_UNKNOWN_SERVICE)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["SOMEIPSRV_BASIC_03"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_basic_03_event_method_id_no_response(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """REQUEST with event method_id (bit 15 set) must not get a RESPONSE.

        The OA test checks a different property: a Notification that the DUT sends has bit 15 set in its method_id.
        This test does not do that check. It checks a related on-wire behavior.
        The specification permits ERROR messages.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, _EVENT_METHOD_ID, client_id=0x0020, session_id=0x0003)
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        response_msgs = [r for r in responses if r.message_type == MessageType.RESPONSE]
        assert not response_msgs, (
            f"DUT sent {len(response_msgs)} RESPONSE message(s) "
            "(message_type=0x80) to a REQUEST with event method_id; "
            "DUT must not send a RESPONSE for event method IDs"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIPSD_00516", "SOMEIPSRV_BASIC_02"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_basic_02_instance_id_reserved_values_excluded(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
    ) -> None:
        """The Instance ID of a real service instance must not be 0x0000 or 0xFFFF.

        Both values are reserved. 0x0000 is not a usable instance identifier.
        0xFFFF means "all instances" in a FindService request.
        The test checks the OfferService entries that the DUT sends.

        The OA procedure first sends a FindService with the wildcard Instance ID (0xFFFF) and checks the Offer.
        The DUT does not send a unicast reply to a FindService. Thus this test checks the cyclic Offer.
        The instance_id is a static configuration value, so the result is the same.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        offers = capture_sd_offers(tc8_host_ip, min_count=1, timeout_secs=5.0)
        assert offers, "No OfferService entries received"

        for offer in offers:
            assert offer.instance_id not in (0x0000, 0xFFFF), (
                f"OfferService instance_id must not be a reserved value "
                f"(0x0000 or 0xFFFF); got 0x{offer.instance_id:04x}"
            )


# ---------------------------------------------------------------------------
# Response field values
# ---------------------------------------------------------------------------


class TestSomeipResponseFields:
    """Tests for the RESPONSE field values."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIPSD_00361", "SOMEIPSRV_ONWIRE_01"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_onwire_01_response_source_address(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """RESPONSE comes from the address and port that the DUT offers.

        The test sends the request to the UDP endpoint in the OfferService. The RESPONSE must come from that endpoint.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        offers = capture_sd_offers(tc8_host_ip, min_count=1, timeout_secs=5.0)
        options = list(offers[0].options_1) + list(offers[0].options_2)
        udp_endpoints = [
            (o.addr, o.port) for o in options if isinstance(o, SDOption_IP4_EndPoint) and o.l4_proto == L4Proto.UDP
        ]
        assert udp_endpoints, f"OfferService carries no UDP IPv4EndpointOption: {options}"
        endpoint = udp_endpoints[0]

        req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0021, session_id=0x0010)
        resp, addr = _send_request_and_receive_with_addr(endpoint, req)

        assert_valid_response(resp, SERVICE_ID, METHOD_ID)
        assert addr == endpoint, (
            f"RESPONSE source {addr[0]}:{addr[1]} is not the offered UDP endpoint {endpoint[0]}:{endpoint[1]}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["SOMEIPSRV_ONWIRE_02"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_onwire_02_method_id_msb_zero_in_response(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """RESPONSE method_id has bit 15 = 0 (not an event)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0021, session_id=0x0011)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert (resp.method_id & 0x8000) == 0, (
            f"RESPONSE method_id bit 15 is set (0x{resp.method_id:04x}); responses must not carry event method IDs"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header", "AS__PRS_SOMEIP_00704"],
        partially_verifies=["AS__PRS_SOMEIP_00922", "SOMEIPSRV_ONWIRE_04"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_onwire_04_request_id_reuse(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Each RESPONSE has the same request_id (client:session) as its REQUEST.

        The test sends two REQUESTs with the same client_id/session_id pair.
        Both RESPONSEs must have the same pair.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        client_id = 0x0001
        session_id = 0x0042

        for _ in range(2):
            req = _build_request(SERVICE_ID, METHOD_ID, client_id=client_id, session_id=session_id)
            resp = send_request_and_receive(tc8_dut_ip, req)

            assert resp.client_id == client_id, (
                f"client_id mismatch: got 0x{resp.client_id:04x}, expected 0x{client_id:04x}"
            )
            assert resp.session_id == session_id, (
                f"session_id mismatch: got 0x{resp.session_id:04x}, expected 0x{session_id:04x}"
            )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00053", "SOMEIPSRV_ONWIRE_06"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_onwire_06_interface_version_echoed(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """RESPONSE interface_version is equal to the REQUEST interface_version.

        The DUT copies the interface_version byte (byte 13) from the REQUEST to the RESPONSE.
        Only the deployed version gets a RESPONSE, thus the request has `MAJOR_VERSION`.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        iface_ver = MAJOR_VERSION
        req = _build_request(
            SERVICE_ID,
            METHOD_ID,
            client_id=0x0021,
            session_id=0x0012,
            interface_version=iface_ver,
        )
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert_valid_response(resp, SERVICE_ID, METHOD_ID)
        assert resp.interface_version == iface_ver, (
            f"interface_version mismatch: got 0x{resp.interface_version:02x}, expected 0x{iface_ver:02x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00922", "AS__PRS_SOMEIP_00191", "SOMEIPSRV_ONWIRE_11"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_onwire_11_normal_response_return_code_ok(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """A normal RESPONSE to a valid REQUEST has return_code E_OK."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0021, session_id=0x0013)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert_return_code(resp, ReturnCode.E_OK)

    @add_test_properties(
        fully_verifies=[],
        partially_verifies=["SOMEIPSRV_ONWIRE_05"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_onwire_05_wrong_protocol_version_rejected(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """A REQUEST with protocol_version=0xFF gets ERROR / E_WRONG_PROTOCOL_VERSION.

        The related AUTOSAR requirement is not in the imported requirements, thus `fully_verifies` is empty.
        `test_tc8_msg_007_wrong_protocol_version_no_crash` sends the same request.
        That test checks only that the DUT is alive.
        This test checks the mandatory ERROR reply.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_wrong_protocol_version_request(SERVICE_ID, METHOD_ID, client_id=0x0021, session_id=0x0014)
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        assert responses, (
            "no reply received for protocol_version=0xFF; an ERROR / E_WRONG_PROTOCOL_VERSION reply is required"
        )
        assert responses[0].message_type == MessageType.ERROR, (
            f"message_type mismatch: got 0x{responses[0].message_type:02x}, expected ERROR (0x{MessageType.ERROR:02x})"
        )
        assert_return_code(responses[0], ReturnCode.E_WRONG_PROTOCOL_VERSION)

    @add_test_properties(
        fully_verifies=[
            "AS__SWS_CM_10302",
            "AS__PRS_SOMEIP_00195",
            "AS__PRS_SOMEIP_00051",
            "AS__PRS_SOMEIP_00190",
            "AS__PRS_SOMEIP_00191",
        ],
        partially_verifies=["SOMEIP_ETS_078"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_078_wrong_protocol_version_returns_error(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """A REQUEST with protocol_version=0xFF gets ERROR / E_WRONG_PROTOCOL_VERSION.

        The DUT must discard the request and send an ERROR with E_WRONG_PROTOCOL_VERSION.
        The Protocol Version is the first check. The reply has Protocol Version 0x01.
        The reply copies Message ID, Request ID and Interface Version from the request, but not the payload.
        The OA pass criteria also accept no reply. The requirements do not, thus a missing reply is a failure.
        The test reports all deviations together.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        client_id, session_id, interface_version = 0x0022, 0x0014, 0x00
        req = _build_wrong_protocol_version_request(
            SERVICE_ID,
            METHOD_ID,
            client_id=client_id,
            session_id=session_id,
            interface_version=interface_version,
        )
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        assert responses, "no reply; an ERROR / E_WRONG_PROTOCOL_VERSION is required"
        reply = responses[0]
        expected = {
            "message_type": (reply.message_type, MessageType.ERROR),
            "return_code": (reply.return_code, ReturnCode.E_WRONG_PROTOCOL_VERSION),
            "protocol_version": (reply.protocol_version, 0x01),
            "service_id": (reply.service_id, SERVICE_ID),
            "method_id": (reply.method_id, METHOD_ID),
            "client_id": (reply.client_id, client_id),
            "session_id": (reply.session_id, session_id),
            "interface_version": (reply.interface_version, interface_version),
            "length": (reply.length, 8),
        }
        deviations = [
            f"{field}: got 0x{int(got):02x}, expected 0x{int(want):02x}"
            for field, (got, want) in expected.items()
            if got != want
        ]
        assert not deviations, "reply to protocol_version=0xFF deviates: " + "; ".join(deviations)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00190", "SOMEIPSRV_RPC_18"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_18_message_id_echoed(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """ERROR response to an unknown method has the Service ID and Method ID of the REQUEST.

        The error message for a request/response method must copy the Message ID (Service ID + Method ID)
        from the REQUEST. This test checks the ERROR reply to an unknown method_id.
        Other tests check a normal RESPONSE to a known method.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, _UNKNOWN_METHOD_ID, client_id=0x0021, session_id=0x0014)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert resp.message_type == MessageType.ERROR, (
            f"message_type mismatch: got 0x{resp.message_type:02x}, expected ERROR (0x{MessageType.ERROR:02x})"
        )
        assert_return_code(resp, ReturnCode.E_UNKNOWN_METHOD)
        assert resp.service_id == SERVICE_ID, (
            f"service_id mismatch in ERROR response: got 0x{resp.service_id:04x}, expected 0x{SERVICE_ID:04x}"
        )
        assert resp.method_id == _UNKNOWN_METHOD_ID, (
            f"method_id mismatch in ERROR response: got 0x{resp.method_id:04x}, expected 0x{_UNKNOWN_METHOD_ID:04x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00190", "SOMEIPSRV_RPC_19"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_19_session_id_echoed_in_error(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """The Request ID (client_id + session_id) of an error response is equal to the Request ID of the request.

        The DUT sends an error response, for example E_UNKNOWN_METHOD for an unknown method_id.
        The full 32-bit Request ID in the response header must be equal to the Request ID of the request.
        Both the client_id and the session_id must be equal.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        session_id = 0x0016
        client_id = 0x0021
        req = _build_request(SERVICE_ID, _UNKNOWN_METHOD_ID, client_id=client_id, session_id=session_id)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert_session_echo(resp, session_id)
        assert_client_echo(resp, client_id)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00190", "SOMEIPSRV_RPC_20"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_20_interface_version_copied_from_request(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """ERROR response has the interface_version of the REQUEST.

        The error message for a request/response method must copy the Interface Version from the REQUEST.
        The test uses interface_version=0x03, not the deployed major version.
        This makes sure that the DUT copies the value from the request.
        PRS_SOMEIP_00195 (Figure 4.21) checks the Interface Version before the Method ID.
        Thus the ERROR has E_WRONG_INTERFACE_VERSION, although the method is also unknown.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        iface_ver = 0x03
        req = _build_request(
            SERVICE_ID,
            _UNKNOWN_METHOD_ID,
            client_id=0x0021,
            session_id=0x0015,
            interface_version=iface_ver,
        )
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert resp.message_type == MessageType.ERROR, (
            f"message_type mismatch: got 0x{resp.message_type:02x}, expected ERROR (0x{MessageType.ERROR:02x})"
        )
        assert_return_code(resp, ReturnCode.E_WRONG_INTERFACE_VERSION)
        assert resp.interface_version == iface_ver, (
            f"interface_version mismatch in ERROR response: "
            f"got 0x{resp.interface_version:02x}, expected 0x{iface_ver:02x}"
        )


# ---------------------------------------------------------------------------
# Fire-and-forget and robustness
# ---------------------------------------------------------------------------


class TestSomeipFireAndForgetAndErrors:
    """Tests for fire-and-forget messages and robustness."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00189", "AS__PRS_SOMEIP_00171", "SOMEIPSRV_RPC_05"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_05_fire_and_forget_no_error(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """REQUEST_NO_RETURN to an unknown method_id gets no RESPONSE and no ERROR.

        A fire-and-forget message must not get an error reply, also when the method_id does not exist.
        The DUT must not reply with E_UNKNOWN_METHOD.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request_no_return(SERVICE_ID, _UNKNOWN_METHOD_ID, client_id=0x0030, session_id=0x0020)
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        assert not responses, (
            f"{len(responses)} message(s) received for "
            "REQUEST_NO_RETURN to an unknown method_id; no RESPONSE or ERROR expected"
        )
        assert_dut_alive(tc8_dut_ip, session_id=0x0106)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00191", "SOMEIPSRV_RPC_06", "SOMEIPSRV_RPC_07"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_06_return_code_upper_bits_zero(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """ERROR response return_code has reserved bits 7-6 = 0.

        The reserved bits of the return_code field are the two most significant bits (mask 0xC0).
        A REQUEST to an unknown method gets an ERROR response, which has a non-zero return_code.
        An E_OK RESPONSE is always 0x00, thus it cannot show the reserved bits.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, _UNKNOWN_METHOD_ID, client_id=0x0030, session_id=0x0021)
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert_return_code(resp, ReturnCode.E_UNKNOWN_METHOD)
        rc_value: int = resp.return_code.value if hasattr(resp.return_code, "value") else int(resp.return_code)
        assert (rc_value & 0xC0) == 0, (
            f"ERROR response return_code reserved bits are not zero: "
            f"0x{rc_value:02x} (bits 7-6 = 0x{(rc_value >> 6) & 0x03:01x})"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00904", "SOMEIPSRV_RPC_07"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_07_request_with_return_code_bits_set(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """The DUT processes a REQUEST normally, also when the return_code field is not zero.

        The server must ignore the return_code in a received REQUEST.
        A REQUEST with return_code=0xC0 (both reserved MSBs set) must get a valid RESPONSE.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request_with_return_code(
            SERVICE_ID,
            METHOD_ID,
            return_code=0xC0,
            client_id=0x0030,
            session_id=0x0022,
        )
        resp = send_request_and_receive(tc8_dut_ip, req)

        assert_valid_response(resp, SERVICE_ID, METHOD_ID)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00904", "AS__PRS_SOMEIP_00539", "SOMEIPSRV_RPC_08"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    @pytest.mark.parametrize("error_return_code", [0x01, 0x1F], ids=["0x01", "0x1F"])
    def test_rpc_08_request_with_error_return_code_no_reply(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        error_return_code: int,
    ) -> None:
        """The DUT does not reply to a REQUEST with a return_code in the error range 0x01-0x1F.

        A server must not process a REQUEST that has an error value in its return_code field.
        The test uses the two limits of the error range in the OA pass criteria.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request_with_return_code(
            SERVICE_ID,
            METHOD_ID,
            return_code=error_return_code,
            client_id=0x0030,
            session_id=0x0023,
        )
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        assert not responses, (
            f"{len(responses)} response(s) received for return_code=0x{error_return_code:02x}; "
            "the DUT must not reply to a REQUEST with a non-zero return_code"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_error_codes"],
        partially_verifies=["AS__PRS_SOMEIP_00190", "SOMEIPSRV_RPC_09"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_09_error_response_no_payload(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """ERROR response to unknown method has length = 8 (no payload).

        The SOME/IP length field counts the bytes from byte 8.
        An error RESPONSE with no payload has length = 8. These are client_id, session_id,
        protocol_version, interface_version, message_type and return_code.
        The REQUEST has a payload. Thus length 8 also shows that the ERROR does not copy the payload.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, _UNKNOWN_METHOD_ID, client_id=0x0030, session_id=0x0024, payload=b"\xaa\xbb")
        resp = send_request_and_receive(tc8_dut_ip, req)

        length_field = resp.length
        assert length_field == 8, (
            f"ERROR response length field = {length_field}, expected 8 (no payload for error responses)"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_malformed"],
        partially_verifies=["SOMEIPSRV_RPC_10"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_10_fire_and_forget_reserved_type_no_error(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Substitute: a fire-and-forget message with a reserved message_type byte gets no ERROR.

        The OA test sends an incorrect message_type (Request or Response) to a fire-and-forget method.
        The DUT must send no error.
        The DUT service has no fire-and-forget method. Its only method, Echo, is a request/response method.
        Thus this test sets byte 14 (message_type) of a REQUEST_NO_RETURN to Echo to the reserved value 0x04.
        The DUT must not send an ERROR.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        raw = _build_request_no_return(SERVICE_ID, METHOD_ID, client_id=0x0030, session_id=0x0025)
        # Set byte 14 (message_type) to the reserved value 0x04.
        patched = raw[:14] + b"\x04" + raw[15:]

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        responses = []
        try:
            sock.bind(("", 0))
            sock.sendto(patched, (tc8_dut_ip, DUT_UNRELIABLE_PORT))
            deadline = time.monotonic() + 2.0
            while time.monotonic() < deadline:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                sock.settimeout(min(remaining, 0.5))
                try:
                    data, _ = sock.recvfrom(65535)
                    msg = parse_someip(data)
                    responses.append(msg)
                except socket.timeout:
                    continue
                except Exception:
                    continue
        finally:
            sock.close()

        assert not responses, (
            f"{len(responses)} message(s) received after sending "
            "fire-and-forget with reserved message_type; the DUT must not send ERROR"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00922", "SOMEIP_ETS_004"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_004_burst_10_sequential_requests(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Each of 10 back-to-back REQUESTs gets a RESPONSE with the correct IDs.

        The test sends 10 REQUESTs with increasing session IDs on one socket.
        It does not wait for a RESPONSE between the REQUESTs.
        Then it receives up to 10 RESPONSEs before one deadline.
        This checks a real burst, not a request/wait/request sequence.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        base_session_id = 0x0100
        session_ids = [base_session_id + i for i in range(10)]

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        responses: list[SomeipResponse] = []
        try:
            sock.bind(("", 0))
            for session_id in session_ids:
                req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0030, session_id=session_id)
                sock.sendto(req, (tc8_dut_ip, DUT_UNRELIABLE_PORT))

            deadline = time.monotonic() + 5.0
            while len(responses) < 10:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                sock.settimeout(remaining)
                try:
                    data, _ = sock.recvfrom(65535)
                except socket.timeout:
                    break
                msg = parse_someip(data)
                responses.append(msg)
        finally:
            sock.close()

        assert len(responses) == 10, f"expected 10 RESPONSEs, got {len(responses)}"
        for resp in responses:
            assert_valid_response(resp, SERVICE_ID, METHOD_ID)

        got_session_ids = {resp.session_id for resp in responses}
        assert got_session_ids == set(session_ids), (
            f"unexpected session IDs in responses: {{{', '.join(f'0x{s:04x}' for s in sorted(got_session_ids))}}}"
        )

    @pytest.mark.xfail(
        strict=True,
        reason=(
            "someipd limitation, seen on the real DUT: the daemon drops a SOME/IP message with length field 0 "
            "and sends no ERROR/E_MALFORMED_MESSAGE response. The daemon does not compare the length field "
            "with the mandatory 8-byte header tail before it discards a message that it cannot parse."
        ),
    )
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00910", "SOMEIP_ETS_054"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_054_empty_payload_request(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Variant: a message with length field 0 is malformed.

        Variant: the message has only the Message ID and the Length, without the rest of the header.
        A length below 8 is not valid, because the length includes the mandatory client_id to return_code fields.
        A correct REQUEST with an empty payload (length 8) is valid and gets E_OK. This test does not send it.
        The OA test requires an ERROR/E_MALFORMED_MESSAGE reply. A silent drop is a failure.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        malformed = build_zero_length_message(SERVICE_ID, METHOD_ID)
        responses = send_request_expect_no_response(tc8_dut_ip, malformed, timeout_secs=2.0)

        assert someipd_dut.poll() is None, (
            "someipd crashed after it received a message with length field = 0 (Message ID and Length only)"
        )

        assert responses, (
            "no response to a message with length field = 0; expected an ERROR reply with E_MALFORMED_MESSAGE"
        )
        is_malformed_error = (
            responses[0].message_type == MessageType.ERROR
            and responses[0].return_code == ReturnCode.E_MALFORMED_MESSAGE
        )
        assert is_malformed_error, (
            f"response to a zero-length-field message was "
            f"message_type=0x{responses[0].message_type:02x} return_code=0x{responses[0].return_code:02x}; "
            "expected ERROR/E_MALFORMED_MESSAGE (a false E_OK is never acceptable)"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00189", "AS__PRS_SOMEIP_00171", "SOMEIP_ETS_059"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_059_fire_and_forget_wrong_service_no_error(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Variant: a fire-and-forget REQUEST to an unknown service_id gets no ERROR reply.

        Variant: the fault is an unknown service_id, not the interface version of resetInterface.
        The test does not do the field steps.
        The DUT service has no resetInterface method. Only the Enhanced Testability Service has it.
        The DUT service has only the field and Echo.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request_no_return(_UNKNOWN_SERVICE_ID, METHOD_ID, client_id=0x0030, session_id=0x0031)
        responses = send_request_expect_no_response(tc8_dut_ip, req, timeout_secs=2.0)

        assert not responses, (
            f"{len(responses)} message(s) received after sending "
            "REQUEST_NO_RETURN to unknown service; the DUT must not send ERROR"
        )
        assert_dut_alive(tc8_dut_ip, session_id=0x0107)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00140", "SOMEIP_ETS_061"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_061_two_sequential_requests(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """Three SOME/IP REQUEST messages in one UDP datagram.

        The test sends three Echo requests in one datagram with ``udp_send_concatenated``.
        The DUT must send three separate responses. Each response has the session ID of its request.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        msg1 = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0030, session_id=0x0040)
        msg2 = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0030, session_id=0x0041)
        msg3 = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0030, session_id=0x0042)

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("", 0))
        try:
            udp_send_concatenated(sock, (tc8_dut_ip, DUT_UNRELIABLE_PORT), [msg1, msg2, msg3])
            responses = udp_receive_responses(sock, count=3, timeout_secs=5.0)
        finally:
            sock.close()

        assert len(responses) == 3, f"expected 3 RESPONSE messages, got {len(responses)}"
        for resp in responses:
            assert_valid_response(resp, SERVICE_ID, METHOD_ID)

        session_ids = {resp.session_id for resp in responses}
        assert session_ids == {0x0040, 0x0041, 0x0042}, (
            f"unexpected session IDs in responses: {{{', '.join(f'0x{s:04x}' for s in sorted(session_ids))}}}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_malformed"],
        partially_verifies=["AS__PRS_SOMEIP_00055", "SOMEIP_ETS_075"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_075_notification_as_request_ignored(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """The DUT does not process a message with type NOTIFICATION as a REQUEST.

        Partial: the test does not compare the return code of an ERROR reply with E_WRONG_MESSAGE_TYPE.
        A NOTIFICATION from a client to a server is not valid.
        The OA test accepts an ERROR reply or no reply.
        Only a RESPONSE is a failure, because it shows that the DUT processed the message as a REQUEST.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        msg = _build_notification_as_request(SERVICE_ID, METHOD_ID, client_id=0x0030, session_id=0x0050)
        responses = send_request_expect_no_response(tc8_dut_ip, msg, timeout_secs=2.0)

        response_msgs = [r for r in responses if r.message_type == MessageType.RESPONSE]
        assert not response_msgs, (
            f"{len(response_msgs)} RESPONSE message(s) received for a "
            "NOTIFICATION message sent to the server; the DUT must not process it as a REQUEST"
        )


# ---------------------------------------------------------------------------
# Big-endian byte order and oversized length field
# ---------------------------------------------------------------------------


class TestSomeipByteOrder:
    """Tests for big-endian response headers and for an oversized length field."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_resp_header"],
        partially_verifies=["AS__PRS_SOMEIP_00368"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_response_header_uses_big_endian_byte_order(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """SOME/IP RESPONSE header fields are big-endian.

        The test sends a REQUEST and compares the raw bytes of the RESPONSE with the parsed header fields.
        The test checks only the byte order of the header.
        The byte order of parameters needs the checkByteOrder method of the ETS. The DUT service does not have it.
        """
        import struct as _struct

        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0022, session_id=0x0090)

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        raw_data: bytes = b""
        parsed: SomeipResponse
        try:
            sock.bind(("", 0))
            sock.sendto(req, (tc8_dut_ip, DUT_UNRELIABLE_PORT))
            sock.settimeout(3.0)
            raw_data, _ = sock.recvfrom(65535)
            parsed = parse_someip(raw_data)
        finally:
            sock.close()

        assert len(raw_data) >= 16, (
            f"RESPONSE is too short for a valid SOME/IP header ({len(raw_data)} bytes, the minimum is 16 bytes)"
        )

        # service_id is bytes 0-1 (big-endian uint16).
        expected_service_id_msb = (parsed.service_id >> 8) & 0xFF
        assert raw_data[0] == expected_service_id_msb, (
            f"service_id MSB byte mismatch: raw byte[0] = 0x{raw_data[0]:02x}, "
            f"expected 0x{expected_service_id_msb:02x} (service_id = 0x{parsed.service_id:04x}). "
            "SOME/IP header must use big-endian byte order."
        )

        # The length field is bytes 4-7 (big-endian uint32).
        raw_length = _struct.unpack_from(">I", raw_data, 4)[0]
        assert raw_length == len(raw_data) - 8, (
            f"length field = {raw_length}, actual payload+header-tail = "
            f"{len(raw_data) - 8}; length field must be big-endian encoded"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__msg_malformed"],
        partially_verifies=["SOMEIP_ETS_058"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_058_oversized_length_field_no_crash(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """SOME/IP message with oversized length field (0xFFFFFFF0) does not crash the DUT.

        Partial: the test does not read the reply to the oversized message.

        The test sets bytes 4-7 of a valid REQUEST to a payload length of 0xFFFFFFF0.
        This is much larger than the UDP datagram. The DUT must discard the malformed message and continue to operate.
        A correct response to a subsequent valid REQUEST shows this.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        wait_for_dut_offer(tc8_host_ip)

        # Make a valid request and set the length field to a very large value.
        valid_req = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0022, session_id=0x0091)
        oversized = bytearray(valid_req)
        oversized[4] = 0xFF
        oversized[5] = 0xFF
        oversized[6] = 0xFF
        oversized[7] = 0xF0

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            sock.bind(("", 0))
            sock.sendto(bytes(oversized), (tc8_dut_ip, DUT_UNRELIABLE_PORT))
        finally:
            sock.close()

        time.sleep(0.3)
        assert someipd_dut.poll() is None, (
            "someipd crashed after receiving a message with oversized length field (0xFFFFFFF0)"
        )

        # Make sure that the DUT still replies to a valid request.
        follow_up = _build_request(SERVICE_ID, METHOD_ID, client_id=0x0022, session_id=0x0092)
        resp = send_request_and_receive(tc8_dut_ip, follow_up)
        assert_valid_response(resp, SERVICE_ID, METHOD_ID)
