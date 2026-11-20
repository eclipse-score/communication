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
"""Event helpers for the TC8 conformance tests.

The helpers subscribe to eventgroups through SD and capture NOTIFICATION messages.
"""

import socket
import time
from typing import List

from someip_itf.codec import L4Proto, MalformedSomeip, MessageType, SdEntryType, SomeipResponse, parse_someip
from someip_itf.tc8.sd_sender import (
    capture_unicast_sd_entries,
    open_sender_socket,
    send_subscribe_eventgroup,
)


def subscribe_and_wait_ack(
    tester_ip: str,
    host_ip: str,
    sd_port: int,
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    major_version: int,
    notif_port: int,
    timeout_secs: float = 5.0,
    ttl: int = 3,
) -> socket.socket:
    """Subscribe to an eventgroup and wait for the Ack.

    Returns the SD socket, which stays open. The caller must close it.
    Raises AssertionError if no Ack arrives.

    *ttl* is the TTL of the SubscribeEventgroup entry, in seconds.
    Use a larger value (for example 30) if the test collects notifications for more than 3 seconds.
    """
    sd_sock = open_sender_socket(tester_ip)
    try:

        def _send_sub() -> None:
            send_subscribe_eventgroup(
                sd_sock,
                (host_ip, sd_port),
                service_id,
                instance_id,
                eventgroup_id,
                major_version,
                subscriber_ip=tester_ip,
                subscriber_port=notif_port,
                ttl=ttl,
            )

        _send_sub()
        entries = capture_unicast_sd_entries(
            sd_sock,
            filter_types=(SdEntryType.SubscribeAck,),
            timeout_secs=timeout_secs,
            resend=_send_sub,
            max_results=1,  # Return at the first Ack. This keeps the subscription TTL.
        )
        acks = [e for e in entries if e.eventgroup_id == eventgroup_id and e.ttl > 0]
        assert acks, f"No SubscribeEventgroupAck received for eventgroup 0x{eventgroup_id:04x}"
    except Exception:
        sd_sock.close()
        raise
    return sd_sock


def capture_notifications(
    sock: socket.socket,
    event_id: int,
    service_id: int,
    count: int = 1,
    timeout_secs: float = 5.0,
) -> List[SomeipResponse]:
    """Capture NOTIFICATION messages for one event on *sock*.

    Returns a maximum of *count* notifications that arrive within *timeout_secs*.
    """
    collected: List[SomeipResponse] = []
    deadline = time.monotonic() + timeout_secs

    while time.monotonic() < deadline and len(collected) < count:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        sock.settimeout(min(remaining, 0.5))
        try:
            data, _ = sock.recvfrom(65535)
        except socket.timeout:
            continue

        try:
            msg = parse_someip(data)
        except MalformedSomeip:
            continue

        if msg.service_id == service_id and msg.method_id == event_id:
            collected.append(msg)

    return collected


def capture_any_notifications(
    sock: socket.socket,
    service_id: int,
    timeout_secs: float = 5.0,
) -> List[SomeipResponse]:
    """Capture all SOME/IP messages for *service_id* on *sock*."""
    collected: List[SomeipResponse] = []
    deadline = time.monotonic() + timeout_secs

    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        sock.settimeout(min(remaining, 0.5))
        try:
            data, _ = sock.recvfrom(65535)
        except socket.timeout:
            continue

        try:
            msg = parse_someip(data)
        except MalformedSomeip:
            continue

        if msg.service_id == service_id:
            collected.append(msg)

    return collected


def drain_socket(sock: socket.socket, quiet_secs: float = 0.1) -> int:
    """Discard datagrams on *sock* until no datagram arrives for *quiet_secs*. Return the number discarded.

    The DUT can send many notifications quickly.
    A capture that stops after the first one leaves the others on the socket.
    Keep *quiet_secs* shorter than the notification cycle of the DUT, so that the drain stops between two cycles.
    """
    discarded = 0
    while True:
        sock.settimeout(quiet_secs)
        try:
            sock.recvfrom(65535)
        except socket.timeout:
            return discarded
        discarded += 1


def assert_notification_header(msg: SomeipResponse, expected_event_id: int) -> None:
    """Assert that a SOME/IP message is a valid NOTIFICATION with the expected event_id."""
    assert msg.message_type == MessageType.NOTIFICATION, (
        f"message_type mismatch: got 0x{msg.message_type:02x}, expected NOTIFICATION (0x{MessageType.NOTIFICATION:02x})"
    )
    assert msg.method_id == expected_event_id, (
        f"event_id mismatch: got 0x{msg.method_id:04x}, expected 0x{expected_event_id:04x}"
    )


def subscribe_and_wait_ack_tcp(
    tester_ip: str,
    host_ip: str,
    sd_port: int,
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    major_version: int,
    notif_port: int,
    timeout_secs: float = 5.0,
) -> socket.socket:
    """Subscribe to an eventgroup with a TCP endpoint and wait for the Ack.

    This is the same as subscribe_and_wait_ack(), but with a TCP endpoint (L4Proto=TCP).
    The DUT then sends the notifications over TCP.
    Returns the SD socket, which stays open. The caller must close it.
    """
    sd_sock = open_sender_socket(tester_ip)
    try:

        def _send_sub() -> None:
            send_subscribe_eventgroup(
                sd_sock,
                (host_ip, sd_port),
                service_id,
                instance_id,
                eventgroup_id,
                major_version,
                subscriber_ip=tester_ip,
                subscriber_port=notif_port,
                l4proto=L4Proto.TCP,
            )

        _send_sub()
        entries = capture_unicast_sd_entries(
            sd_sock,
            filter_types=(SdEntryType.SubscribeAck,),
            timeout_secs=timeout_secs,
            resend=_send_sub,
            max_results=1,
        )
        acks = [e for e in entries if e.eventgroup_id == eventgroup_id and e.ttl > 0]
        assert acks, f"No SubscribeEventgroupAck received for TCP eventgroup 0x{eventgroup_id:04x}"
    except Exception:
        sd_sock.close()
        raise
    return sd_sock
