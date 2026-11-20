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
"""Helpers that make malformed SD packets for the TC8 robustness tests.

Each public function makes or sends a SOME/IP-SD UDP datagram with an intentional protocol error.
The DUT must not crash, stop or go into an incorrect state when it receives these packets.

Wire format
-----------
SOME/IP header (16 bytes):
  [0-1]   service_id  = 0xFFFF
  [2-3]   method_id   = 0x8100
  [4-7]   length      (big-endian, bytes from byte 8 onward)
  [8-9]   client_id   = 0x0001
  [10-11] session_id  (big-endian)
  [12]    protocol_version  = 0x01
  [13]    interface_version = 0x01
  [14]    message_type      = 0x02 (NOTIFICATION)
  [15]    return_code       = 0x00

SD payload (starts at byte 16):
  [0]    flags    (0xC0 = reboot | unicast)
  [1-3]  reserved (0x000000)
  [4-7]  entries_array_length (big-endian)
  [8+]   entry bytes (entries_array_length bytes)
  [...] options_array_length (4 bytes big-endian)
  [...]  option bytes

SOME/IP length = 8 + len(SD payload).
"""

import itertools
import socket
import struct
from typing import Optional, Tuple

from someip_itf.codec import SD_METHOD_ID, SD_SERVICE_ID

# ---------------------------------------------------------------------------
# Session counter of this module. It is not the counter of sd_sender.
# ---------------------------------------------------------------------------

_malformed_session: itertools.count = itertools.count(start=200)


def _next_session() -> int:
    """Return the next session ID in the range 1-65535. Skip 0."""
    val = next(_malformed_session) & 0xFFFF
    return val or 200  # skip 0


# ---------------------------------------------------------------------------
# Raw packet builder
# ---------------------------------------------------------------------------

_SOMEIP_SD_HEADER_FMT = ">HHIHHBBBB"  # 16 bytes total
_SD_FLAGS_REBOOT_UNICAST = 0xC0
_SD_FLAGS_NONE = 0x00


def _build_someip_header(session_id: int, payload_len: int) -> bytes:
    """Build the 16-byte SOME/IP header for an SD notification."""
    length = 8 + payload_len
    return struct.pack(
        _SOMEIP_SD_HEADER_FMT,
        SD_SERVICE_ID,
        SD_METHOD_ID,
        length,  # length
        0x0001,  # client_id
        session_id,  # session_id
        0x01,  # protocol_version
        0x01,  # interface_version (SD uses 0x01)
        0x02,  # message_type = NOTIFICATION
        0x00,  # return_code = E_OK
    )


def _build_sd_payload(
    flags: int,
    entries_bytes: bytes,
    options_bytes: bytes,
    entries_length_override: Optional[int] = None,
    options_length_override: Optional[int] = None,
) -> bytes:
    """Build the SD payload: flags, array lengths, entries and options."""
    entries_len = entries_length_override if entries_length_override is not None else len(entries_bytes)
    options_len = options_length_override if options_length_override is not None else len(options_bytes)
    return (
        struct.pack(">B3xI", flags, entries_len)  # flags(1)+reserved(3)+entries_len(4)
        + entries_bytes
        + struct.pack(">I", options_len)
        + options_bytes
    )


def build_raw_sd_packet(
    flags: int = _SD_FLAGS_REBOOT_UNICAST,
    entries_bytes: bytes = b"",
    options_bytes: bytes = b"",
    entries_length_override: Optional[int] = None,
    options_length_override: Optional[int] = None,
    session_id: int = 0,
    someip_length_override: Optional[int] = None,
    someip_service_id_override: Optional[int] = None,
    someip_method_id_override: Optional[int] = None,
) -> bytes:
    """Build a full SOME/IP-SD packet. The *_override* parameters can replace fields.

    Use the *_override* parameters to put specific protocol errors in the packet.
    """
    if session_id == 0:
        session_id = _next_session()
    sd_payload = _build_sd_payload(
        flags,
        entries_bytes,
        options_bytes,
        entries_length_override,
        options_length_override,
    )
    raw = bytearray(_build_someip_header(session_id, len(sd_payload)) + sd_payload)
    if someip_length_override is not None:
        struct.pack_into(">I", raw, 4, someip_length_override)
    if someip_service_id_override is not None:
        struct.pack_into(">H", raw, 0, someip_service_id_override)
    if someip_method_id_override is not None:
        struct.pack_into(">H", raw, 2, someip_method_id_override)
    return bytes(raw)


# ---------------------------------------------------------------------------
# SD entry byte builders
# ---------------------------------------------------------------------------


def _find_service_entry_bytes(
    service_id: int,
    instance_id: int = 0xFFFF,
    major_version: int = 0xFF,
    ttl: int = 3,
    minor_version: int = 0xFFFFFFFF,
) -> bytes:
    """Build a 16-byte FindService (Type 0) SD entry."""
    ttl_3b = struct.pack(">I", ttl)[1:]  # 3 bytes big-endian
    return (
        bytes([0x00, 0x00, 0x00, 0x00])
        + struct.pack(">HH", service_id, instance_id)
        + bytes([major_version])
        + ttl_3b
        + struct.pack(">I", minor_version)
    )


def _subscribe_entry_bytes(
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    major_version: int = 0x01,
    ttl: int = 3,
    num_opts: int = 1,
    idx_opt: int = 0,
    reserved: bytes = b"\x00\x00",
) -> bytes:
    """Build a 16-byte SubscribeEventgroup (Type 0x06) SD entry.

    The default *major_version* is 1, the major version of the TC8 service.
    Thus, the DUT rejects the entry only because of the error that the caller adds.

    Wire layout (big-endian):
      byte 0:    type = 0x06
      byte 1:    index_first_option_run
      byte 2:    index_second_option_run
      byte 3:    (num_options_1 << 4) | num_options_2
      bytes 4-5: service_id
      bytes 6-7: instance_id
      byte 8:    major_version
      bytes 9-11: TTL (3 bytes)
      bytes 12-13: reserved (usually 0)
      bytes 14-15: eventgroup_id
    """
    ttl_3b = struct.pack(">I", ttl)[1:]
    return (
        bytes([0x06, idx_opt, 0x00, (num_opts << 4)])
        + struct.pack(">HH", service_id, instance_id)
        + bytes([major_version])
        + ttl_3b
        + reserved
        + struct.pack(">H", eventgroup_id & 0xFFFF)
    )


def _endpoint_option_bytes(
    ip: str,
    port: int,
    l4proto: int = 0x11,  # 0x11 = UDP, 0x06 = TCP
    length_override: Optional[int] = None,
) -> bytes:
    """Build a 12-byte IPv4 Endpoint Option.

    Wire layout:
      [0-1]  length = 0x0009
      [2]    type   = 0x04
      [3]    reserved = 0x00
      [4-7]  IPv4 address
      [8]    reserved = 0x00
      [9]    l4proto
      [10-11] port
    """
    length_field = length_override if length_override is not None else 0x0009
    addr_bytes = socket.inet_aton(ip)
    return struct.pack(">HBB", length_field, 0x04, 0x00) + addr_bytes + struct.pack(">BBH", 0x00, l4proto, port)


def _unknown_option_bytes(option_type: int = 0x77, content_len: int = 4) -> bytes:
    """Build an SD option with an unknown type byte."""
    length_field = content_len + 1  # The length includes the type byte.
    padding = bytes(content_len)
    return struct.pack(">HBB", length_field, option_type, 0x00) + padding


# ---------------------------------------------------------------------------
# Public senders of malformed packets
# ---------------------------------------------------------------------------


def send_sd_empty_entries(
    sock: socket.socket,
    dest: Tuple[str, int],
) -> None:
    """Send an SD packet with entries_array_length=0 (no entries, no options).

    The DUT must not crash. It must not send an unexpected OfferService.
    """
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=b"",
        options_bytes=b"",
    )
    sock.sendto(pkt, dest)


def send_sd_find_with_options(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send a FindService entry with an endpoint option.

    The DUT must ignore options on FindService entries.
    The DUT must answer the FindService as usual.
    """
    # The FindService entry has num_options_1=1, so that it refers to an option.
    ttl_3b = struct.pack(">I", 3)[1:]
    entry_bytes = (
        bytes([0x00, 0x00, 0x00, 0x10])  # type=Find, idx_1=0, idx_2=0, num_1=1/num_2=0
        + struct.pack(">HH", service_id, 0xFFFF)
        + bytes([0xFF])
        + ttl_3b
        + struct.pack(">I", 0xFFFFFFFF)
    )
    opt_bytes = _endpoint_option_bytes(host_ip, subscriber_port)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_entries_length_wrong(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send two Subscribe entries that share one endpoint option, with an entries array length of 16."""
    entry_bytes = (
        _subscribe_entry_bytes(service_id=service_id, instance_id=instance_id, eventgroup_id=eventgroup_id, num_opts=1)
        * 2
    )
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=_endpoint_option_bytes(host_ip, subscriber_port),
        entries_length_override=16,
    )
    sock.sendto(pkt, dest)


def send_sd_entry_refs_more_options(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
    ttl: int = 3,
) -> None:
    """Send a SubscribeEventgroup entry with num_options_1=3, but the options array has only 1 option.

    The DUT must discard the Subscribe and can send a NAck. It must not crash. ttl=0 makes it a StopSubscribe.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        ttl=ttl,
        num_opts=3,
    )
    opt_bytes = _endpoint_option_bytes(host_ip, subscriber_port)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_entry_unknown_option_type(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
) -> None:
    """Send a SubscribeEventgroup with an option of the unknown type 0x77.

    The DUT can send a NAck or discard the Subscribe. It must not crash.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        num_opts=1,
    )
    opt_bytes = _unknown_option_bytes(option_type=0x77, content_len=4)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_entry_same_option_twice(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send one SubscribeEventgroup entry that refers to two options of the same kind (two IPv4EndpointOptions).

    The DUT can send a SubscribeEventgroupNAck or ignore the request.
    It must not crash. It must not accept the subscription with an Ack.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id, instance_id=instance_id, eventgroup_id=eventgroup_id, num_opts=2
    )
    opt_bytes = _endpoint_option_bytes(host_ip, subscriber_port) + _endpoint_option_bytes(host_ip, subscriber_port + 1)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_two_options_one_unknown_type(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send a SubscribeEventgroup entry that refers to two options.

    One option is a valid IPv4EndpointOption. The other option has the unknown type 0x77.
    Thus, one of the two options that the DUT needs is not usable.
    The DUT must reject the subscription with an explicit SubscribeEventgroupNAck.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id, instance_id=instance_id, eventgroup_id=eventgroup_id, num_opts=2
    )
    opt_bytes = _endpoint_option_bytes(host_ip, subscriber_port) + _unknown_option_bytes(
        option_type=0x77, content_len=4
    )
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_option_length_too_long(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
    option_length_override: int,
) -> None:
    """Send a Subscribe with one IPv4 endpoint option that has the length *option_length_override*, not 9."""
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        num_opts=1,
    )
    opt_bytes = _endpoint_option_bytes(host_ip, subscriber_port, length_override=option_length_override)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_option_length_too_short(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send an IPv4EndpointOption with the length field 4. A UDP endpoint option needs 9.

    The DUT must reject the subscription with an explicit SubscribeEventgroupNAck, or ignore the request.
    """
    send_sd_option_length_too_long(
        sock,
        dest,
        service_id,
        instance_id,
        eventgroup_id,
        host_ip,
        subscriber_port,
        option_length_override=0x0004,
    )


def send_sd_option_pair_unaligned(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send two endpoint options. One option is 5 bytes longer than usual, the other is 5 bytes shorter.

    The long option has 5 more bytes after its 12 bytes (length field 14, the "TCP option" case).
    The short option has 5 bytes less (length field 4, the "UDP option" case).
    The total size of the two options looks correct, but each option is malformed.
    The DUT must reject the subscription with an explicit SubscribeEventgroupNAck.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id, instance_id=instance_id, eventgroup_id=eventgroup_id, num_opts=2
    )
    addr_bytes = socket.inet_aton(host_ip)
    padded_opt = (
        struct.pack(">HBB", 14, 0x04, 0x00)
        + addr_bytes
        + struct.pack(">BBH", 0x00, 0x06, subscriber_port)
        + b"\x00" * 5  # Filler bytes for the larger length field.
    )
    # Length 4 covers the reserved byte and 3 address bytes. The two options have 24 bytes, as two valid options do.
    cut_opt = struct.pack(">HBB", 4, 0x04, 0x00) + addr_bytes[:3]
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=padded_opt + cut_opt,
    )
    sock.sendto(pkt, dest)


def send_sd_two_option_array_length(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
    options_length_override: int,
    someip_length_override: Optional[int] = None,
) -> None:
    """Send a Subscribe that refers to a UDP and a TCP endpoint option, with an incorrect options array length.

    One case uses 12 and makes the SOME/IP length 12 bytes shorter.
    Other cases use 0x28 or 2.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id, instance_id=instance_id, eventgroup_id=eventgroup_id, num_opts=2
    )
    opt_bytes = _endpoint_option_bytes(host_ip, subscriber_port, l4proto=0x11) + _endpoint_option_bytes(
        host_ip, subscriber_port, l4proto=0x06
    )
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
        options_length_override=options_length_override,
        someip_length_override=someip_length_override,
    )
    sock.sendto(pkt, dest)


def send_sd_subscribe_port_zero(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
) -> None:
    """Send a SubscribeEventgroup with an IPv4EndpointOption that has port 0.

    The entry refers to a valid option. Only the port is 0.
    The DUT can parse the option, but it cannot send events to this port.
    The DUT must reject the subscription with an explicit SubscribeEventgroupNAck.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        num_opts=1,
    )
    opt_bytes = _endpoint_option_bytes(host_ip, port=0)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_subscribe_endpoint_ip(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    endpoint_ip: str,
    subscriber_port: int,
) -> None:
    """Send a SubscribeEventgroup with the endpoint IP *endpoint_ip*.

    Example values: unspecified ("0.0.0.0"), not reachable ("169.254.1.1"), the DUT address,
    or an invalid address ("111.111.111.111").
    The DUT must send a NAck or discard the Subscribe. It must not crash.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        num_opts=1,
    )
    opt_bytes = _endpoint_option_bytes(endpoint_ip, subscriber_port)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_subscribe_wrong_l4proto(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
    l4proto: int = 0x00,
) -> None:
    """Send a SubscribeEventgroup with an unknown L4 protocol byte in the endpoint option.

    The option has l4proto=0x00. This is not UDP (0x11) and not TCP (0x06).
    The DUT must send a NAck or discard the Subscribe. It must not crash.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        num_opts=1,
    )
    opt_bytes = _endpoint_option_bytes(host_ip, subscriber_port, l4proto=l4proto)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_subscribe_endpoint_reserved_bits_set(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send a SubscribeEventgroup with a valid IPv4EndpointOption (type 0x04) with reserved bytes that are not zero.

    The two reserved bytes are at offsets [3] and [8] of the 12-byte option.
    The option type is known and all other fields are correct.
    The DUT must ignore the reserved bits and accept the subscription with a SubscribeEventgroupAck.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        num_opts=1,
    )
    addr_bytes = socket.inet_aton(host_ip)
    opt_bytes = (
        struct.pack(">HBB", 0x0009, 0x04, 0xAB)  # The reserved byte after the type is 0xAB, not 0x00.
        + addr_bytes
        + struct.pack(">BBH", 0xCD, 0x11, subscriber_port)  # The reserved byte before the protocol is 0xCD.
    )
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_subscribe_wrong_major_version(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
    major_version: int = 0xAB,
    ttl: int = 3,
) -> None:
    """Send a SubscribeEventgroup for the correct service, instance and eventgroup, with an unknown major version.

    The DUT must reject the subscription with an explicit SubscribeEventgroupNAck. ttl=0 makes it a StopSubscribe.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        major_version=major_version,
        ttl=ttl,
        num_opts=1,
    )
    opt_bytes = _endpoint_option_bytes(host_ip, subscriber_port)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_subscribe_wrong_someip_length(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
    length_override: int,
) -> None:
    """Send a Subscribe with one endpoint option and the SOME/IP length field *length_override*, not 48."""
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id, instance_id=instance_id, eventgroup_id=eventgroup_id, num_opts=1
    )
    opt_bytes = _endpoint_option_bytes(host_ip, subscriber_port)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
        someip_length_override=length_override,
    )
    sock.sendto(pkt, dest)


def send_sd_wrong_someip_message_id(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send a valid SubscribeEventgroup with the service ID 0x1234 in the SOME/IP Message ID, not 0xFFFF."""
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id, instance_id=instance_id, eventgroup_id=eventgroup_id, num_opts=1
    )
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=_endpoint_option_bytes(host_ip, subscriber_port),
        someip_service_id_override=0x1234,
    )
    sock.sendto(pkt, dest)


def send_sd_entries_length_too_short(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send a full 16-byte Subscribe entry and its endpoint option, with an entries array length of 8.

    Thus, the DUT reads the options array length from the middle of the entry.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id, instance_id=instance_id, eventgroup_id=eventgroup_id, num_opts=1
    )
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=_endpoint_option_bytes(host_ip, subscriber_port),
        entries_length_override=8,
    )
    sock.sendto(pkt, dest)


def send_sd_oversized_entries_length(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send a Subscribe entry and an endpoint option, with an entries array length of 0xFFFF."""
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id, instance_id=instance_id, eventgroup_id=eventgroup_id, num_opts=1
    )
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=_endpoint_option_bytes(host_ip, subscriber_port),
        entries_length_override=0xFFFF,
    )
    sock.sendto(pkt, dest)


def send_sd_entries_length_too_long_padded(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
) -> None:
    """Send a Subscribe entry, 24 zero bytes and an endpoint option, with an entries array length of 40.

    The packet has all 40 bytes, but 40 is not a multiple of the entry size of 16 bytes.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id, instance_id=instance_id, eventgroup_id=eventgroup_id, num_opts=1
    )
    padded = entry_bytes + b"\x00" * 24
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=padded,
        options_bytes=_endpoint_option_bytes(host_ip, subscriber_port),
        entries_length_override=len(padded),
    )
    sock.sendto(pkt, dest)


def send_sd_empty_option(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
) -> None:
    """Send a SubscribeEventgroup with an IPv4Endpoint option that has the length field 0.

    An option with length=0 has nothing after its 2-byte length field. It has no type byte and no content.
    The DUT must reject the subscription with an explicit SubscribeEventgroupNAck.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        num_opts=1,
    )
    opt_bytes = struct.pack(">H", 0x0000)  # length=0: no type byte and no content follow.
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)


def send_sd_empty_options_array(
    sock: socket.socket,
    dest: Tuple[str, int],
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
) -> None:
    """Send a SubscribeEventgroup entry that refers to one option (num_options_1=1), with options_array_length 0.

    Thus, the option is not in the packet.
    The DUT must reject the subscription with an explicit SubscribeEventgroupNAck.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        num_opts=1,
    )
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=b"",
        options_length_override=0,
    )
    sock.sendto(pkt, dest)


def send_sd_subscribe_nonexistent_service(
    sock: socket.socket,
    dest: Tuple[str, int],
    unknown_service_id: int,
    instance_id: int,
    eventgroup_id: int,
    host_ip: str,
    subscriber_port: int,
    ttl: int = 3,
) -> None:
    """Send a SubscribeEventgroup for a service_id that the DUT does not offer.

    The DUT must send no SubscribeAck, or a NAck. It must not crash. ttl=0 makes it a StopSubscribe.
    """
    entry_bytes = _subscribe_entry_bytes(
        service_id=unknown_service_id,
        instance_id=instance_id,
        eventgroup_id=eventgroup_id,
        ttl=ttl,
        num_opts=1,
    )
    opt_bytes = _endpoint_option_bytes(host_ip, subscriber_port)
    pkt = build_raw_sd_packet(
        flags=_SD_FLAGS_REBOOT_UNICAST,
        entries_bytes=entry_bytes,
        options_bytes=opt_bytes,
    )
    sock.sendto(pkt, dest)
