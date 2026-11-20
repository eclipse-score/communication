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
"""TC8 field conformance tests.

TestField (getter 0x0001, setter 0x0002, notifier 0x0777, eventgroup 0x4455) is a uint16_t.
The tests set two-byte values and read them back byte for byte.
They read the values from a GET response and from the notification that a SET causes.
"""

import socket
import time

import pytest
from attribute_plugin import add_test_properties
from someip_itf.codec import ReturnCode
from someip_itf.tc8.constants import (
    DUT_RELIABLE_PORT,
    DUT_UNRELIABLE_PORT,
    EVENTGROUP_ID,
    INSTANCE_ID,
    MAJOR_VERSION,
    SD_PORT,
    SERVICE_ID,
)
from someip_itf.tc8.dut_lifecycle import TargetStack
from someip_itf.tc8.events import capture_notifications, subscribe_and_wait_ack
from someip_itf.tc8.fields import send_get_field, send_get_field_tcp, send_set_field, send_set_field_tcp
from someip_itf.tc8.sd import capture_sd_offers

_LGSOMEIP_4_XFAIL_REASON = (
    "[lg-someip #4] the daemon matches a Subscribe to an existing subscription by IP address only, so a second "
    "subscriber endpoint on the same IP address gets no route"
)

# ---------------------------------------------------------------------------
# Module-level configuration
# ---------------------------------------------------------------------------

_METHOD_GET: int = 0x0001
_METHOD_SET: int = 0x0002
#: Wire notification ID = 0x8000 | the event ID in the model (0x0777).
_EVENT_NOTIFIER: int = 0x8777
#: Initial value of TestField (kInitialFieldValue). The tests read it before a test sets the field.
_FIELD_INITIAL_VALUE: bytes = b"\xde\xad"

#: TestStaticField: read-only, set one time at startup.
_METHOD_STATIC_GET: int = 0x0003
#: Its initial value (kStaticFieldValue). This field has no setter, thus it does not change.
_STATIC_FIELD_VALUE: bytes = b"\xfe\xed"


class TestFieldInitialValue:
    """The DUT sends the cached field value to a new subscriber."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__fld_initial_value"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_fld_001_initial_notification_on_subscribe(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A subscription to the TestField eventgroup causes an immediate NOTIFICATION."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        try:
            capture_sd_offers(tc8_host_ip, min_count=1, timeout_secs=5.0)
        except (TimeoutError, OSError):
            pytest.skip("DUT did not offer service within timeout")

        sd_sock = None
        notif_sock = None
        try:
            notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            notif_sock.bind((tc8_tester_ip, 0))
            notif_port = notif_sock.getsockname()[1]
            sd_sock = subscribe_and_wait_ack(
                tc8_tester_ip,
                tc8_dut_ip,
                SD_PORT,
                SERVICE_ID,
                INSTANCE_ID,
                EVENTGROUP_ID,
                MAJOR_VERSION,
                notif_port=notif_port,
            )
            notifs = capture_notifications(
                notif_sock,
                _EVENT_NOTIFIER,
                SERVICE_ID,
                count=1,
                timeout_secs=1.5,
            )
            assert notifs, "No initial NOTIFICATION received after the subscription to the TestField eventgroup"
            received_payload = bytes(notifs[0].payload) if notifs[0].payload else b""
            assert received_payload == _FIELD_INITIAL_VALUE, (
                f"initial NOTIFICATION payload mismatch: got {received_payload!r}, "
                f"expected the cached field value {_FIELD_INITIAL_VALUE!r}"
            )
        finally:
            if sd_sock:
                sd_sock.close()
            if notif_sock:
                notif_sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__fld_initial_value"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    # Not strict: the case fails only inside the TTL of the previous case's subscription.
    @pytest.mark.xfail(strict=False, reason=_LGSOMEIP_4_XFAIL_REASON)
    def test_tc8_fld_002_initial_value_within_one_second(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """The initial notification for a field arrives in approximately 1 s after the subscribe ACK."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sd_sock = None
        notif_sock = None
        try:
            notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            notif_sock.bind((tc8_tester_ip, 0))
            notif_port = notif_sock.getsockname()[1]
            sd_sock = subscribe_and_wait_ack(
                tc8_tester_ip,
                tc8_dut_ip,
                SD_PORT,
                SERVICE_ID,
                INSTANCE_ID,
                EVENTGROUP_ID,
                MAJOR_VERSION,
                notif_port=notif_port,
            )
            ack_time = time.monotonic()
            notifs = capture_notifications(
                notif_sock,
                _EVENT_NOTIFIER,
                SERVICE_ID,
                count=1,
                timeout_secs=1.5,
            )
            elapsed = time.monotonic() - ack_time
            assert notifs, "No initial NOTIFICATION received within 1.5 s of subscribe"
            assert elapsed <= 1.0, (
                f"initial NOTIFICATION arrived {elapsed * 1000:.0f} ms after the subscribe ACK; expected within ~1 s"
            )
            received_payload = bytes(notifs[0].payload) if notifs[0].payload else b""
            assert received_payload == _FIELD_INITIAL_VALUE, (
                f"initial NOTIFICATION payload mismatch: got {received_payload!r}, "
                f"expected the cached field value {_FIELD_INITIAL_VALUE!r}"
            )
        finally:
            if sd_sock:
                sd_sock.close()
            if notif_sock:
                notif_sock.close()


class TestFieldGetSet:
    """The getter returns the current value. The setter updates the value and sends a notification."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__fld_get_set"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_fld_003_getter_returns_current_value(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """A GET request returns a RESPONSE with the current field value."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        resp = send_get_field(
            tc8_dut_ip,
            SERVICE_ID,
            _METHOD_GET,
            DUT_UNRELIABLE_PORT,
            client_id=0x0030,
            session_id=0x0001,
        )

        assert resp.return_code == ReturnCode.E_OK, (
            f"GET returned code 0x{resp.return_code.value:02x} ({resp.return_code.name}), expected E_OK"
        )
        received_payload = bytes(resp.payload) if resp.payload else b""
        assert received_payload == _FIELD_INITIAL_VALUE, (
            f"GET payload mismatch: got {received_payload!r}, expected {_FIELD_INITIAL_VALUE!r}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__fld_get_set"],
        partially_verifies=["SOMEIPSRV_RPC_11"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_fld_004_setter_updates_value_and_notifies(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A SET request updates the field and sends a notification to each subscriber."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        new_value = b"\xca\xfe"

        sd_sock = None
        notif_sock = None
        try:
            notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            notif_sock.bind((tc8_tester_ip, 0))
            notif_port = notif_sock.getsockname()[1]
            sd_sock = subscribe_and_wait_ack(
                tc8_tester_ip,
                tc8_dut_ip,
                SD_PORT,
                SERVICE_ID,
                INSTANCE_ID,
                EVENTGROUP_ID,
                MAJOR_VERSION,
                notif_port=notif_port,
            )

            # Drain the initial-value notification that the subscribe causes.
            capture_notifications(notif_sock, _EVENT_NOTIFIER, SERVICE_ID, count=1, timeout_secs=1.5)

            set_resp = send_set_field(
                tc8_dut_ip,
                SERVICE_ID,
                _METHOD_SET,
                new_value,
                DUT_UNRELIABLE_PORT,
                client_id=0x0030,
                session_id=0x0002,
            )
            assert set_resp.return_code == ReturnCode.E_OK, (
                f"SET returned code 0x{set_resp.return_code.value:02x} ({set_resp.return_code.name}), expected E_OK"
            )
            set_resp_payload = bytes(set_resp.payload) if set_resp.payload else b""
            assert set_resp_payload == new_value, (
                f"SET RESPONSE payload mismatch: got {set_resp_payload!r}, expected the echoed new value {new_value!r}"
            )

            notifs = capture_notifications(
                notif_sock,
                _EVENT_NOTIFIER,
                SERVICE_ID,
                count=1,
                timeout_secs=3.0,
            )
            assert notifs, "No NOTIFICATION received after SET, each subscriber must get a notification"
            received_payload = bytes(notifs[0].payload) if notifs[0].payload else b""
            assert received_payload == new_value, (
                f"Notification payload mismatch: got {received_payload!r}, expected {new_value!r}"
            )
        finally:
            if sd_sock:
                sd_sock.close()
            if notif_sock:
                notif_sock.close()


class TestStaticFieldGetter:
    """The getter of TestStaticField, called directly with request/response.

    someipd does not send the notifier of TestStaticField again. This test does not use the notifier.
    The request/response path does not go through the LoLa event-subscription bridge.
    Thus that limitation does not apply to this test.
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__fld_get_set"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_static_field_getter_returns_seeded_value(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """A GET request to TestStaticField returns its initial value, which does not change after startup."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        resp = send_get_field(
            tc8_dut_ip,
            SERVICE_ID,
            _METHOD_STATIC_GET,
            DUT_UNRELIABLE_PORT,
            client_id=0x0031,
            session_id=0x0001,
        )

        assert resp.return_code == ReturnCode.E_OK, (
            f"TestStaticField GET returned code 0x{resp.return_code.value:02x} ({resp.return_code.name}), expected E_OK"
        )
        received_payload = bytes(resp.payload) if resp.payload else b""
        assert received_payload == _STATIC_FIELD_VALUE, (
            f"TestStaticField GET payload mismatch: got {received_payload!r}, expected {_STATIC_FIELD_VALUE!r}"
        )


class TestFieldTcpTransport:
    """Field GET/SET over TCP (reliable transport).

    These tests repeat the UDP checks of TestFieldGetSet over TCP.
    No numbered field-specific test case covers the TCP transport.
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__tcp_transport"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_fld_003_tcp_field_getter(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """A GET request over TCP returns the current field value."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        resp = send_get_field_tcp(
            tc8_dut_ip,
            SERVICE_ID,
            _METHOD_GET,
            DUT_RELIABLE_PORT,
            client_id=0x0040,
            session_id=0x0010,
        )

        assert resp.return_code == ReturnCode.E_OK, (
            f"TCP GET returned code 0x{resp.return_code.value:02x} ({resp.return_code.name}), expected E_OK"
        )
        received_payload = bytes(resp.payload) if resp.payload else b""
        assert received_payload == _FIELD_INITIAL_VALUE, (
            f"TCP GET payload mismatch: got {received_payload!r}, expected {_FIELD_INITIAL_VALUE!r}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__tcp_transport"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_fld_004_tcp_field_setter(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """A SET request over TCP updates the value. The next GET reads it back."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        new_value = b"\xbe\xef"
        set_resp = send_set_field_tcp(
            tc8_dut_ip,
            SERVICE_ID,
            _METHOD_SET,
            new_value,
            DUT_RELIABLE_PORT,
            client_id=0x0040,
            session_id=0x0011,
        )

        assert set_resp.return_code == ReturnCode.E_OK, (
            f"TCP SET returned code 0x{set_resp.return_code.value:02x} ({set_resp.return_code.name}), expected E_OK"
        )
        set_resp_payload = bytes(set_resp.payload) if set_resp.payload else b""
        assert set_resp_payload == new_value, (
            f"TCP SET RESPONSE payload mismatch: got {set_resp_payload!r}, expected the echoed new value {new_value!r}"
        )

        get_resp = send_get_field_tcp(
            tc8_dut_ip,
            SERVICE_ID,
            _METHOD_GET,
            DUT_RELIABLE_PORT,
            client_id=0x0040,
            session_id=0x0012,
        )
        received_payload = bytes(get_resp.payload) if get_resp.payload else b""
        assert received_payload == new_value, (
            f"TCP GET after SET payload mismatch: got {received_payload!r}, expected {new_value!r}"
        )
