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
"""SD helpers for the TC8 tests.

The helpers make and send SD messages (for example FindService and SubscribeEventgroup).
They also capture unicast SD replies and SOME/IP notifications.
"""

import dataclasses
import itertools
import socket
import time
from typing import Callable, List, Optional, Sequence, Tuple

from scapy.contrib.automotive.someip import SDOption_IP4_EndPoint, SDOption_IP4_Multicast

from someip_itf.codec import L4Proto, MalformedSomeip, SdEntry, SdEntryType, SomeipResponse, build_sd, parse_someip
from someip_itf.tc8.constants import SD_PORT
from someip_itf.tc8.sd import parse_sd_message

# ---------------------------------------------------------------------------
# Socket management
# ---------------------------------------------------------------------------


def open_sender_socket(local_ip: str) -> socket.socket:
    """Open a UDP socket at ``(local_ip, SD_PORT)`` to send and receive SD messages.

    The socket binds to ``SD_PORT``, because the SOME/IP stack drops SD messages from other ports.
    ``local_ip`` must not be the DUT address. Use the ``tester_ip`` fixture.
    Multicast loopback is on, so that a local DUT receives the packets.
    The caller must close the socket.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except AttributeError:
        pass  # Some platforms do not have SO_REUSEPORT.
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(local_ip))
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
    sock.bind((local_ip, SD_PORT))
    return sock


# ---------------------------------------------------------------------------
# SD packet serialization
# ---------------------------------------------------------------------------


# SD session counter. It increases for each message, so that the DUT does not see duplicates.
# The session_id starts at 0x0001, because 0x0000 is reserved.
# NOTE: The counter is not thread-safe. The tests must run one after the other.
_session_counter = itertools.count(start=1)


def _next_session_id() -> int:
    """Return the next SD session ID. The ID wraps at 16 bits."""
    return next(_session_counter) & 0xFFFF or 1  # skip 0 (reserved)


def _build_sd_packet(
    entries: Sequence[SdEntry],
    session_id: int = 0,
    flag_unicast: bool = True,
    flag_reboot: bool = False,
) -> bytes:
    """Wrap one or more SD entries into a SOME/IP-SD UDP datagram.

    If *session_id* is 0 (default), the helper uses the session counter.
    *flag_reboot* has an effect only together with a *session_id* that goes back to a value
    that the DUT did not see from this sender before.
    The ``options_1`` and ``options_2`` runs of all entries share one options array.
    Two identical runs use the same options.
    """
    if session_id == 0:
        session_id = _next_session_id()
    return build_sd(entries, session_id=session_id, flag_unicast=flag_unicast, flag_reboot=flag_reboot)


def send_sd_entries(
    sock: socket.socket,
    sd_dest: Tuple[str, int],
    entries: Sequence[SdEntry],
    session_id: int = 0,
) -> None:
    """Send many valid SD entries in one SD datagram."""
    sock.sendto(_build_sd_packet(entries, session_id), sd_dest)


# ---------------------------------------------------------------------------
# Send helpers
# ---------------------------------------------------------------------------


def send_find_service(
    sock: socket.socket,
    sd_dest: Tuple[str, int],
    service_id: int,
    instance_id: int = 0xFFFF,
    major_version: int = 0xFF,
    minor_version: int = 0xFFFFFFFF,
    session_id: int = 0,
    flag_unicast: bool = True,
    flag_reboot: bool = False,
) -> None:
    """Send a FindService SD message to *sd_dest*.

    *flag_unicast* and *flag_reboot* set the Unicast Flag and the Reboot Flag of the SD header.
    """
    entry = SdEntry(
        sd_type=SdEntryType.FindService,
        service_id=service_id,
        instance_id=instance_id,
        major_version=major_version,
        ttl=3,
        minver_or_counter=minor_version,
    )
    sock.sendto(_build_sd_packet((entry,), session_id, flag_unicast=flag_unicast, flag_reboot=flag_reboot), sd_dest)


def build_offer_service(
    service_id: int,
    instance_id: int,
    major_version: int,
    minor_version: int,
    provider_ip: str,
    provider_port: int,
    ttl: int = 3,
    session_id: int = 0,
    l4proto: L4Proto = L4Proto.UDP,
) -> bytes:
    """Return an OfferService SD datagram for *provider_ip*/*provider_port*. The caller sends or changes it.

    The endpoint option has the address and port of the service.
    The daemon sends its Subscribe entry and the notifications to this endpoint.
    If *session_id* is 0, the helper uses the next value of the shared session counter.
    """
    endpoint_opt = SDOption_IP4_EndPoint(
        addr=provider_ip,
        l4_proto=l4proto,
        port=provider_port,
    )
    entry = SdEntry(
        sd_type=SdEntryType.OfferService,
        service_id=service_id,
        instance_id=instance_id,
        major_version=major_version,
        ttl=ttl,
        minver_or_counter=minor_version,
        options_1=(endpoint_opt,),
    )
    return _build_sd_packet((entry,), session_id)


def send_subscribe_ack(
    sock: socket.socket,
    sd_dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    major_version: int,
    ttl: int = 3,
    session_id: int = 0,
    counter: int = 0,
    multicast_group: Optional[Tuple[str, int]] = None,
    options: Optional[Sequence] = None,
) -> None:
    """Send a SubscribeEventgroupAck (or a NAck if ``ttl=0``) to *sd_dest*.

    With *multicast_group*, the Ack has one IPv4 Multicast Option with the event address.
    With *options*, the Ack has exactly these options, for example options that a correct server does not send.
    Without the two, the Ack has no option.
    The daemon accepts the Ack only if *counter* is equal to the counter of the Subscribe.
    """
    if (multicast_group is not None) and (options is not None):
        raise ValueError("send_subscribe_ack takes multicast_group or options, not both")
    if multicast_group is not None:
        group_ip, group_port = multicast_group
        options = (SDOption_IP4_Multicast(addr=group_ip, l4_proto=L4Proto.UDP, port=group_port),)
    entry = SdEntry(
        sd_type=SdEntryType.SubscribeAck,
        service_id=service_id,
        instance_id=instance_id,
        major_version=major_version,
        ttl=ttl,
        minver_or_counter=((counter & 0x0F) << 16) | (eventgroup_id & 0xFFFF),
        options_1=tuple(options or ()),
    )
    sock.sendto(_build_sd_packet((entry,), session_id), sd_dest)


def build_subscribe_entry(
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    major_version: int,
    subscriber_ip: str,
    subscriber_port: int,
    ttl: int = 3,
    l4proto: L4Proto = L4Proto.UDP,
) -> SdEntry:
    """Build a SubscribeEventgroup SD entry (or a StopSubscribe if ``ttl=0``)."""
    endpoint_opt = SDOption_IP4_EndPoint(
        addr=subscriber_ip,
        l4_proto=l4proto,
        port=subscriber_port,
    )
    return SdEntry(
        sd_type=SdEntryType.Subscribe,
        service_id=service_id,
        instance_id=instance_id,
        major_version=major_version,
        ttl=ttl,
        minver_or_counter=eventgroup_id & 0xFFFF,
        options_1=(endpoint_opt,),
    )


def send_subscribe_eventgroup(
    sock: socket.socket,
    sd_dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    major_version: int,
    subscriber_ip: str,
    subscriber_port: int,
    ttl: int = 3,
    session_id: int = 0,
    l4proto: L4Proto = L4Proto.UDP,
) -> None:
    """Send a SubscribeEventgroup (or a StopSubscribe if ``ttl=0``)."""
    entry = build_subscribe_entry(
        service_id, instance_id, eventgroup_id, major_version, subscriber_ip, subscriber_port, ttl, l4proto
    )
    sock.sendto(_build_sd_packet((entry,), session_id), sd_dest)


def send_subscribe_eventgroup_reserved_set(
    sock: socket.socket,
    sd_dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    major_version: int,
    subscriber_ip: str,
    subscriber_port: int,
    ttl: int = 3,
    reserved_value: int = 0x0F,
) -> None:
    """Send a SubscribeEventgroup with *reserved_value* in its 12 reserved bits (bits 31-20 of ``minver_or_counter``).

    The DUT must send a NAck or ignore the Subscribe.
    """
    entry = build_subscribe_entry(
        service_id, instance_id, eventgroup_id, major_version, subscriber_ip, subscriber_port, ttl
    )
    entry = dataclasses.replace(entry, minver_or_counter=((reserved_value & 0x0FFF) << 20) | (eventgroup_id & 0xFFFF))
    sock.sendto(_build_sd_packet((entry,)), sd_dest)


# ---------------------------------------------------------------------------
# Capture helpers
# ---------------------------------------------------------------------------


def capture_unicast_sd_entries(
    sock: socket.socket,
    filter_types: Optional[Tuple[SdEntryType, ...]] = None,
    timeout_secs: float = 5.0,
    resend: Optional[Callable[[], None]] = None,
    resend_interval_secs: float = 1.5,
    max_results: Optional[int] = None,
) -> List[SdEntry]:
    """Receive SD entries on *sock* within *timeout_secs*.

    If *filter_types* is set, the function returns only these entry types.

    If *resend* is set, the function calls it every *resend_interval_secs* until a matching entry arrives.
    A real SD client also sends FindService and Subscribe messages periodically.

    If *max_results* is set, the function returns when it has this number of matching entries.
    Thus, it does not wait for the full *timeout_secs*.
    This is important for tests in which the subscription TTL must not expire.
    """
    collected: List[SdEntry] = []
    deadline = time.monotonic() + timeout_secs
    next_resend = (time.monotonic() + resend_interval_secs) if resend else None

    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break

        # Stop when there are max_results matching entries.
        if max_results is not None and len(collected) >= max_results:
            break

        # Send again if no entry matches and the interval is over.
        if resend and next_resend and not collected and time.monotonic() >= next_resend:
            resend()
            next_resend = time.monotonic() + resend_interval_secs

        sock.settimeout(min(remaining, 0.5))
        try:
            data, _ = sock.recvfrom(65535)
        except socket.timeout:
            continue

        for entry in _parse_sd_entries(data):
            if filter_types is None or entry.sd_type in filter_types:
                collected.append(entry)

    return collected


def capture_some_ip_messages(
    sock: socket.socket,
    service_id: int,
    timeout_secs: float,
    method_id: Optional[int] = None,
) -> List[SomeipResponse]:
    """Receive SOME/IP messages for *service_id* on *sock* within *timeout_secs*.

    If *method_id* is set, the function returns only messages with this method ID.
    """
    return [msg for msg, _addr in capture_some_ip_messages_with_addr(sock, service_id, timeout_secs, method_id)]


def capture_some_ip_messages_with_addr(
    sock: socket.socket,
    service_id: int,
    timeout_secs: float,
    method_id: Optional[int] = None,
) -> List[Tuple[SomeipResponse, Tuple[str, int]]]:
    """Receive SOME/IP messages for *service_id* on *sock* within *timeout_secs*, each with its sender address.

    If *method_id* is set, the function returns only messages with this method ID.
    """
    collected: List[Tuple[SomeipResponse, Tuple[str, int]]] = []
    deadline = time.monotonic() + timeout_secs

    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        sock.settimeout(min(remaining, 0.5))
        try:
            data, addr = sock.recvfrom(65535)
        except socket.timeout:
            continue

        try:
            msg = parse_someip(data)
        except MalformedSomeip:
            continue

        if msg.service_id == service_id and (method_id is None or msg.method_id == method_id):
            collected.append((msg, addr))

    return collected


# ---------------------------------------------------------------------------
# Internal parsing
# ---------------------------------------------------------------------------


def _parse_sd_entries(data: bytes) -> List[SdEntry]:
    """Parse a UDP payload and return all SD entries of all types, with the options resolved."""
    parsed = parse_sd_message(data)
    return list(parsed[1].entries) if parsed is not None else []
