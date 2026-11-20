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
"""SD multicast capture and OfferService parsing for the TC8 tests.

The other TC8 helpers get SD parsing and multicast sockets from this module.
The sockets are blocking sockets.
Multicast needs an interface that is not loopback. ``host_ip`` must be the address of such an interface.
"""

import socket
import struct
import time
from typing import List, Optional, Tuple

from someip_itf.codec import SdEntry, SdEntryType, SdMessage, SomeipResponse, parse_sd_datagram
from someip_itf.tc8.constants import SD_MULTICAST_ADDR, SD_PORT


def create_udp_socket(bind_addr: str = "", port: int = 0) -> socket.socket:
    """Create a UDP socket. If *bind_addr* or *port* is set, bind it to *bind_addr*:*port*."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if bind_addr or port:
        sock.bind((bind_addr, port))
    return sock


def open_multicast_socket(
    host_ip: str,
    multicast_group: str = SD_MULTICAST_ADDR,
    port: int = SD_PORT,
) -> socket.socket:
    """Open a UDP socket and join *multicast_group*. The caller must close it."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except AttributeError:
        pass
    sock.bind(("", port))
    group_bytes = socket.inet_aton(multicast_group)
    iface_bytes = socket.inet_aton(host_ip)
    mreq = struct.pack("4s4s", group_bytes, iface_bytes)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    return sock


def parse_sd_message(data: bytes) -> Optional[Tuple[SomeipResponse, SdMessage]]:
    """Parse a UDP payload as a SOME/IP-SD message, with the options resolved.

    Returns ``None`` if *data* is not a SOME/IP-SD message that can be decoded.
    This is the case for a message ID that is not the SD message ID, or for data that ``parse_sd`` rejects.
    To get only ``OfferService`` entries, use `parse_sd_offers`.
    """
    return parse_sd_datagram(data)


def parse_sd_offers(data: bytes) -> List[SdEntry]:
    """Parse a UDP payload and return all OfferService entries.

    Returns ``[]`` if *data* is not a valid SOME/IP-SD message.
    """
    parsed = parse_sd_message(data)
    if parsed is None:
        return []

    _, sd_header = parsed
    return [entry for entry in sd_header.entries if entry.sd_type == SdEntryType.OfferService]


def capture_sd_offers(
    host_ip: str,
    multicast_group: str = SD_MULTICAST_ADDR,
    port: int = SD_PORT,
    min_count: int = 1,
    timeout_secs: float = 5.0,
) -> List[SdEntry]:
    """Join the SD multicast group and collect OfferService entries.

    Returns when *min_count* entries are captured.
    Raises ``TimeoutError`` if not sufficient entries arrive within *timeout_secs*.
    Raises ``OSError`` if the helper cannot open the multicast socket.
    """
    sock = open_multicast_socket(host_ip, multicast_group, port)
    try:
        deadline = time.monotonic() + timeout_secs
        collected: List[SdEntry] = []

        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            sock.settimeout(min(remaining, 1.0))
            try:
                data, _ = sock.recvfrom(65535)
            except socket.timeout:
                continue

            offers = parse_sd_offers(data)
            collected.extend(offers)

            if len(collected) >= min_count:
                return collected

    finally:
        sock.close()

    if len(collected) < min_count:
        raise TimeoutError(
            f"Captured only {len(collected)} SD OFFER entries within "
            f"{timeout_secs:.1f}s (expected at least {min_count})"
        )

    return collected  # pragma: no cover — only if min_count == 0


def collect_sd_messages_from_socket(
    sock: socket.socket,
    count: int,
    timeout_secs: float,
) -> List[Tuple[SomeipResponse, SdMessage]]:
    """Collect *count* full SD messages from an open socket, with all entry types.

    Each result has the SOME/IP header and the SD header.
    Thus, it keeps `SomeipResponse.session_id` and `SdMessage.flag_reboot`, which the reboot detection needs.
    `parse_sd_offers` discards these fields. *count* must be at least 1.

    Raises ``TimeoutError`` if fewer than *count* messages arrive within *timeout_secs*.
    """
    assert count >= 1, "collect_sd_messages_from_socket: count must be at least 1"

    deadline = time.monotonic() + timeout_secs
    collected: List[Tuple[SomeipResponse, SdMessage]] = []

    while time.monotonic() < deadline and len(collected) < count:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        sock.settimeout(min(remaining, 1.0))
        try:
            data, _ = sock.recvfrom(65535)
        except socket.timeout:
            continue

        parsed = parse_sd_message(data)
        if parsed is not None:
            collected.append(parsed)

    if len(collected) < count:
        raise TimeoutError(
            f"Captured only {len(collected)} SD message(s) within {timeout_secs:.1f}s (expected at least {count})"
        )

    return collected
