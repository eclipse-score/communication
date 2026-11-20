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
"""TC8 event notification tests.

The DUT sends the TestField notifier again every 500 ms, also when the value does not change.
The UDP tests subscribe to it and use it as usual event traffic.
The format, rate and subscription checks do not depend on the fact that the value is also a field.
The change-only test needs a notifier that stays silent when the value does not change.
Thus it subscribes to TestStaticField. No code sends TestStaticField again after startup.
The TCP test subscribes to TestTcpEvent, the only event that uses TCP.
"""

import socket
import time

import pytest
from attribute_plugin import add_test_properties
from someip_itf.codec import MalformedSomeip, MessageType, parse_someip
from someip_itf.tc8.constants import (
    DUT_RELIABLE_PORT,
    EVENTGROUP_ID,
    INSTANCE_ID,
    MAJOR_VERSION,
    MULTICAST_EVENT_PORT,
    MULTICAST_EVENTGROUP_ID,
    SD_MULTICAST_ADDR,
    SD_PORT,
    SERVICE_ID,
)
from someip_itf.tc8.dut_lifecycle import TargetStack
from someip_itf.tc8.events import (
    assert_notification_header,
    capture_any_notifications,
    capture_notifications,
    drain_socket,
    subscribe_and_wait_ack,
    subscribe_and_wait_ack_tcp,
)
from someip_itf.tc8.sd import capture_sd_offers, open_multicast_socket
from someip_itf.tc8.sd_sender import send_subscribe_eventgroup
from someip_itf.tcp import tcp_connect, tcp_receive_response

_LGSOMEIP_4_XFAIL_REASON = (
    "[lg-someip #4] the daemon matches a Subscribe to an existing subscription by IP address only, so a second "
    "subscriber endpoint on the same IP address gets no route"
)

# ---------------------------------------------------------------------------
# Module-level configuration
# ---------------------------------------------------------------------------

#: The notifier and eventgroups of TestField. The tests use this traffic as "the event".
#: Wire notification ID = 0x8000 | the event ID in the model (0x0777).
_EVENT_NOTIFIER: int = 0x8777

#: TestStaticField: set one time, not sent again.
_STATIC_FIELD_EVENTGROUP_ID: int = 0x4480
#: Wire notification ID = 0x8000 | the event ID in the model (0x0779).
_STATIC_FIELD_NOTIFIER: int = 0x8779

#: TestTcpEvent: the only TCP event of the deployment, in a separate eventgroup.
_TCP_EVENTGROUP_ID: int = 0x4475
#: Wire notification ID = 0x8000 | the event ID in the model (0x0778).
_TCP_EVENT_NOTIFIER: int = 0x8778

#: Period of the notify loop in the DUT (kNotifyCycle).
_NOTIFY_CYCLE_MS: float = 500.0


class TestEventNotificationFormat:
    """Notification format and delivery rate."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__evt_subscription"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_evt_001_notification_message_type(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """The event notification has message_type = NOTIFICATION (0x02)."""
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
                timeout_secs=5.0,
            )
            assert notifs, "No NOTIFICATION received after subscription"
            assert notifs[0].message_type == MessageType.NOTIFICATION, (
                f"message_type = 0x{notifs[0].message_type:02x}, "
                f"expected NOTIFICATION (0x{MessageType.NOTIFICATION:02x})"
            )
        finally:
            if sd_sock:
                sd_sock.close()
            if notif_sock:
                notif_sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__evt_subscription"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    # Not strict: the case fails only inside the TTL of the previous case's subscription.
    @pytest.mark.xfail(strict=False, reason=_LGSOMEIP_4_XFAIL_REASON)
    def test_tc8_evt_002_correct_event_id(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """The notification has the correct event_id in the method_id field."""
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
            notifs = capture_notifications(
                notif_sock,
                _EVENT_NOTIFIER,
                SERVICE_ID,
                count=1,
                timeout_secs=5.0,
            )
            assert notifs, "No NOTIFICATION received after subscription"
            assert_notification_header(notifs[0], _EVENT_NOTIFIER)
        finally:
            if sd_sock:
                sd_sock.close()
            if notif_sock:
                notif_sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__evt_subscription"],
        partially_verifies=["SOMEIPSRV_RPC_15"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_15_cyclic_notification_rate(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """Cyclic event notifications arrive at the configured cycle period."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        tolerance_factor = 3.0
        min_interval = (_NOTIFY_CYCLE_MS / 1000.0) / tolerance_factor
        max_interval = (_NOTIFY_CYCLE_MS / 1000.0) * tolerance_factor

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
                ttl=30,  # 30 s > 10 s observation window. The subscription stays active for the full window.
            )

            timestamps: list[float] = []
            deadline = time.monotonic() + 10.0
            while time.monotonic() < deadline and len(timestamps) < 4:
                remaining = deadline - time.monotonic()
                notif_sock.settimeout(min(remaining, 2.0))
                try:
                    data, _ = notif_sock.recvfrom(65535)
                except socket.timeout:
                    continue
                try:
                    msg = parse_someip(data)
                except MalformedSomeip:
                    continue
                if msg.service_id == SERVICE_ID and msg.method_id == _EVENT_NOTIFIER:
                    timestamps.append(time.monotonic())

            assert len(timestamps) >= 4, (
                f"received only {len(timestamps)} notification(s) within 10 s; "
                f"expected at least 4 at a ~{_NOTIFY_CYCLE_MS:.0f} ms cycle"
            )

            # Skip the first gap. The initial event on subscribe can make it shorter.
            intervals = [timestamps[i + 1] - timestamps[i] for i in range(1, len(timestamps) - 1)]
            for idx, interval in enumerate(intervals):
                assert min_interval <= interval <= max_interval, (
                    f"notification interval {idx + 1} = {interval * 1000:.0f} ms; "
                    f"expected [{min_interval * 1000:.0f} ms, {max_interval * 1000:.0f} ms] "
                    f"(configured cycle: {_NOTIFY_CYCLE_MS:.0f} ms)"
                )
        finally:
            if sd_sock:
                sd_sock.close()
            if notif_sock:
                notif_sock.close()


class TestEventSubscriptionGating:
    """Only subscribed endpoints get notifications, and only after the subscription."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__evt_subscription"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_evt_003_notification_only_to_subscriber(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """The subscribed endpoint receives notifications. An endpoint without a subscription does not."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sd_sock = None
        sub_sock = None
        unsub_sock = None
        try:
            sub_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sub_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sub_sock.bind((tc8_tester_ip, 0))
            sub_port = sub_sock.getsockname()[1]

            unsub_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            unsub_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            unsub_sock.bind((tc8_tester_ip, 0))
            sd_sock = subscribe_and_wait_ack(
                tc8_tester_ip,
                tc8_dut_ip,
                SD_PORT,
                SERVICE_ID,
                INSTANCE_ID,
                EVENTGROUP_ID,
                MAJOR_VERSION,
                notif_port=sub_port,
            )

            notifs = capture_notifications(sub_sock, _EVENT_NOTIFIER, SERVICE_ID, count=1, timeout_secs=5.0)
            assert notifs, "No notification on the subscribed socket"

            stray = capture_any_notifications(unsub_sock, SERVICE_ID, timeout_secs=2.0)
            assert not stray, f"{len(stray)} notification(s) on the unsubscribed socket"
        finally:
            if sd_sock:
                sd_sock.close()
            if sub_sock:
                sub_sock.close()
            if unsub_sock:
                unsub_sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__evt_subscription"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_evt_004_no_notification_before_subscribe(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """No notifications arrive before the subscription."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        # The DUT sends a notification every 500 ms. Thus a 3 s window is sufficient to find a leak.
        listen_sock = None
        try:
            listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listen_sock.bind((tc8_tester_ip, 0))
            stray = capture_any_notifications(listen_sock, SERVICE_ID, timeout_secs=3.0)
            assert not stray, f"{len(stray)} notification(s) received without a subscription"
        finally:
            if listen_sock:
                listen_sock.close()


class TestEventStopSubscribe:
    """StopSubscribeEventgroup stops the notifications."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__evt_subscription", "comp_req__tc8_conformance__sd_sub_lifecycle"],
        partially_verifies=["SOMEIP_ETS_108"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_evt_006_stop_subscribe_ceases_notifications(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """Notifications stop after StopSubscribeEventgroup (TTL=0).

        Variant: the 500 ms cyclic notifications of TestField replace the initial event and the triggered TestEventUINT8.
        """
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
            notifs = capture_notifications(notif_sock, _EVENT_NOTIFIER, SERVICE_ID, count=1, timeout_secs=5.0)
            assert notifs, "pre-condition failed: no notifications before StopSubscribe"
            # Notifications that the DUT sent before the stop do not count.
            drain_socket(notif_sock)

            send_subscribe_eventgroup(
                sd_sock,
                (tc8_dut_ip, SD_PORT),
                SERVICE_ID,
                INSTANCE_ID,
                EVENTGROUP_ID,
                MAJOR_VERSION,
                subscriber_ip=tc8_tester_ip,
                subscriber_port=notif_port,
                ttl=0,
            )

            # The DUT sends a notification every 500 ms. Thus a 4 s window finds a leak.
            post = capture_any_notifications(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert not post, f"{len(post)} notification(s) after StopSubscribeEventgroup"
        finally:
            if sd_sock:
                sd_sock.close()
            if notif_sock:
                notif_sock.close()


class TestMulticastEventDelivery:
    """Notifications for a multicast eventgroup arrive on the multicast address.

    The SubscribeAck for this eventgroup has no IPv4MulticastOption. This is a real, separate gap in the SD data.
    This test checks only the multicast delivery of the data, which works.
    """

    @pytest.mark.network
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__evt_subscription"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_evt_005_multicast_notification_delivery(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """Notifications arrive on the multicast group after the subscription to the multicast eventgroup."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        try:
            capture_sd_offers(tc8_host_ip, min_count=1, timeout_secs=5.0)
        except (TimeoutError, OSError):
            pytest.skip("DUT did not offer service within timeout")

        # Join the multicast group before the subscription. Thus the first notification is not lost.
        try:
            mcast_sock = open_multicast_socket(
                tc8_host_ip, multicast_group=SD_MULTICAST_ADDR, port=MULTICAST_EVENT_PORT
            )
        except OSError as exc:
            pytest.skip(f"Cannot join the event multicast group on {tc8_host_ip}: {exc}")

        sd_sock = None
        try:
            sd_sock = subscribe_and_wait_ack(
                tc8_tester_ip,
                tc8_dut_ip,
                SD_PORT,
                SERVICE_ID,
                INSTANCE_ID,
                MULTICAST_EVENTGROUP_ID,
                MAJOR_VERSION,
                notif_port=mcast_sock.getsockname()[1],
            )
            notifs = capture_any_notifications(mcast_sock, SERVICE_ID, timeout_secs=5.0)
            assert notifs, (
                f"No SOME/IP notification received on the multicast group within 5 s after "
                f"the subscription to eventgroup 0x{MULTICAST_EVENTGROUP_ID:04x}"
            )
            assert notifs[0].message_type == MessageType.NOTIFICATION, (
                f"message_type mismatch on the multicast socket: "
                f"got 0x{notifs[0].message_type:02x}, expected NOTIFICATION (0x02)"
            )
        finally:
            if sd_sock:
                sd_sock.close()
            mcast_sock.close()


class TestFieldChangeOnlyNotification:
    """A notifier that does not change sends its initial value and then stays silent."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__evt_subscription"],
        partially_verifies=["SOMEIPSRV_RPC_16"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_16_static_field_notifies_only_once(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """TestStaticField sends one notification on subscribe, then no more notifications."""
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
                _STATIC_FIELD_EVENTGROUP_ID,
                MAJOR_VERSION,
                notif_port=notif_port,
            )

            initial = capture_notifications(notif_sock, _STATIC_FIELD_NOTIFIER, SERVICE_ID, count=1, timeout_secs=5.0)
            assert initial, (
                "No initial-value notification received after the subscription to "
                f"eventgroup 0x{_STATIC_FIELD_EVENTGROUP_ID:04x}"
            )

            # No code calls Update() on TestStaticField again. Thus the next window must stay silent.
            subsequent = capture_notifications(
                notif_sock, _STATIC_FIELD_NOTIFIER, SERVICE_ID, count=1, timeout_secs=3.0
            )
            assert not subsequent, (
                f"{len(subsequent)} unexpected notification(s) of TestStaticField "
                f"(0x{_STATIC_FIELD_NOTIFIER:04x}) received within 3 s; its value does not change, thus no further "
                "notification is expected"
            )
        finally:
            if sd_sock:
                sd_sock.close()
            if notif_sock:
                notif_sock.close()


class TestEventTcpNotification:
    """Event notification delivery over TCP (reliable transport).

    The DUT must use a single TCP connection for all notifications.
    The test subscribes over the TCP connection of the service and expects the notifications on it.

    The test subscribes to TestTcpEvent, the only event of the deployment that uses TCP.
    someipd sends an event only over the transport of its deployment.
    Thus a UDP-only notifier, such as the TestField notifier, does not reach a subscriber with only a TCP endpoint.
    TestTcpEvent has no initial value. Thus the received notification is the next one from the notify cycle of the DUT.
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__tcp_transport"],
        partially_verifies=["SOMEIPSRV_RPC_02"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_rpc_02_tcp_event_notification_delivery(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A notification arrives on TCP after a subscription with a TCP endpoint."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        try:
            capture_sd_offers(tc8_host_ip, min_count=1, timeout_secs=5.0)
        except (TimeoutError, OSError):
            pytest.skip("DUT did not offer service within timeout")

        sd_sock = None
        tcp_sock = None
        try:
            # The client must open the TCP connection before the DUT accepts a reliable subscription.
            # The subscribe uses the source port of this connection as the delivery endpoint.
            tcp_sock = tcp_connect(tc8_dut_ip, DUT_RELIABLE_PORT)
            local_tcp_port = tcp_sock.getsockname()[1]

            sd_sock = subscribe_and_wait_ack_tcp(
                tc8_tester_ip,
                tc8_dut_ip,
                SD_PORT,
                SERVICE_ID,
                INSTANCE_ID,
                _TCP_EVENTGROUP_ID,
                MAJOR_VERSION,
                notif_port=local_tcp_port,
            )

            msg = tcp_receive_response(tcp_sock, timeout_secs=8.0)
            assert msg.message_type == MessageType.NOTIFICATION, (
                f"message_type mismatch over TCP: got 0x{msg.message_type:02x}, expected NOTIFICATION (0x02)"
            )
            assert msg.method_id == _TCP_EVENT_NOTIFIER, (
                f"event_id mismatch over TCP: got 0x{msg.method_id:04x}, expected 0x{_TCP_EVENT_NOTIFIER:04x}"
            )
        finally:
            if sd_sock:
                sd_sock.close()
            if tcp_sock:
                tcp_sock.close()
