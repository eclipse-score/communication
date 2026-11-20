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

"""Decode and encode SOME/IP and SOME/IP-SD messages with scapy, for the functional-test helpers.

Parsed messages and SD entries use the names ``service_id``, ``message_type``, ``minver_or_counter``,
``options_1`` ... in place of the scapy field names. The TC8 and ETS helpers read these names.
SD options are scapy packets (``SDOption_IP4_EndPoint`` ...), or ``Raw`` for an unknown or truncated option.
The codec uses the option Length to find the option bounds, because scapy option classes ignore it.
"""

from __future__ import annotations

import dataclasses
import enum
import struct
from typing import Optional, Sequence, Tuple

from scapy.contrib.automotive.someip import (
    SD,
    SDENTRY_TYPE_EVTGRP_SUBSCRIBE,
    SDENTRY_TYPE_EVTGRP_SUBSCRIBE_ACK,
    SDENTRY_TYPE_SRV_FINDSERVICE,
    SDENTRY_TYPE_SRV_OFFERSERVICE,
    SDOPTION_CFG_TYPE,
    SDOPTION_IP4_ENDPOINT_TYPE,
    SDOPTION_IP4_MCAST_TYPE,
    SDOPTION_IP4_SDENDPOINT_TYPE,
    SDOPTION_IP6_ENDPOINT_TYPE,
    SDOPTION_IP6_MCAST_TYPE,
    SDOPTION_IP6_SDENDPOINT_TYPE,
    SDOPTION_LOADBALANCE_TYPE,
    SOMEIP,
    SDEntry_EventGroup,
    SDEntry_Service,
    SDOption_Config,
    SDOption_IP4_EndPoint,
    SDOption_IP4_Multicast,
    SDOption_IP4_SD_EndPoint,
    SDOption_IP6_EndPoint,
    SDOption_IP6_Multicast,
    SDOption_IP6_SD_EndPoint,
    SDOption_LoadBalance,
)
from scapy.packet import Packet, Raw

SD_SERVICE_ID = SD.SOMEIP_MSGID_SRVID
SD_METHOD_ID = SD.SOMEIP_MSGID_SUBID
SD_INTERFACE_VERSION = SD.SOMEIP_IFACE_VER

_HEADER_SIZE = 16
_SD_HEADER_SIZE = 12
_SD_ENTRY_SIZE = 16
_SD_OPTION_HEADER_SIZE = 3  # Length (2) and Type (1); the option's Length counts every byte after them.
_SD_FLAG_REBOOT = 0x80
_SD_FLAG_UNICAST = 0x40
_SD_CLIENT_ID = 0x0000
_MAX_RUN_LENGTH = 0x0F  # Each run's option count is a 4-bit field.
_MAX_TTL = 0xFFFFFF
_MAX_RUN_INDEX = 0xFF  # Each run's first-option index is an 8-bit field.


class MessageType(enum.IntEnum):
    """SOME/IP message type field values."""

    REQUEST = 0x00
    REQUEST_NO_RETURN = 0x01
    NOTIFICATION = 0x02
    REQUEST_ACK = 0x40
    REQUEST_NO_RETURN_ACK = 0x41
    NOTIFICATION_ACK = 0x42
    RESPONSE = 0x80
    ERROR = 0x81
    RESPONSE_ACK = 0xC0
    ERROR_ACK = 0xC1


class ReturnCode(enum.IntEnum):
    """SOME/IP return code field values, with the E2E codes."""

    E_OK = 0x00
    E_NOT_OK = 0x01
    E_UNKNOWN_SERVICE = 0x02
    E_UNKNOWN_METHOD = 0x03
    E_NOT_READY = 0x04
    E_NOT_REACHABLE = 0x05
    E_TIMEOUT = 0x06
    E_WRONG_PROTOCOL_VERSION = 0x07
    E_WRONG_INTERFACE_VERSION = 0x08
    E_MALFORMED_MESSAGE = 0x09
    E_WRONG_MESSAGE_TYPE = 0x0A
    E_E2E_REPEATED = 0x0B
    E_E2E_WRONG_SEQUENCE = 0x0C
    E_E2E = 0x0D
    E_E2E_NOT_AVAILABLE = 0x0E
    E_E2E_NO_NEW_DATA = 0x0F


class SdEntryType(enum.IntEnum):
    """SOME/IP-SD entry type field values."""

    FindService = SDENTRY_TYPE_SRV_FINDSERVICE
    OfferService = SDENTRY_TYPE_SRV_OFFERSERVICE
    Subscribe = SDENTRY_TYPE_EVTGRP_SUBSCRIBE
    SubscribeAck = SDENTRY_TYPE_EVTGRP_SUBSCRIBE_ACK


class L4Proto(enum.IntEnum):
    """L4-Protocol field values of an SD endpoint option."""

    TCP = 0x06
    UDP = 0x11


# parse_someip accepts only these values. It rejects the E2E return codes 0x0B-0x0F.
_PARSE_MESSAGE_TYPES = frozenset(int(t) for t in MessageType)
_PARSE_RETURN_CODES = frozenset(range(0x00, 0x0B))

_SERVICE_ENTRY_TYPES = (SdEntryType.FindService, SdEntryType.OfferService)
_EVENTGROUP_ENTRY_TYPES = (SdEntryType.Subscribe, SdEntryType.SubscribeAck)

_OPTION_CLASSES = {
    SDOPTION_CFG_TYPE: SDOption_Config,
    SDOPTION_LOADBALANCE_TYPE: SDOption_LoadBalance,
    SDOPTION_IP4_ENDPOINT_TYPE: SDOption_IP4_EndPoint,
    SDOPTION_IP6_ENDPOINT_TYPE: SDOption_IP6_EndPoint,
    SDOPTION_IP4_MCAST_TYPE: SDOption_IP4_Multicast,
    SDOPTION_IP6_MCAST_TYPE: SDOption_IP6_Multicast,
    SDOPTION_IP4_SDENDPOINT_TYPE: SDOption_IP4_SD_EndPoint,
    SDOPTION_IP6_SDENDPOINT_TYPE: SDOption_IP6_SD_EndPoint,
}
# Scapy raises an error when the bytes are fewer than the fixed fields of an option class.
_OPTION_MIN_SIZE = {option_type: len(cls()) for option_type, cls in _OPTION_CLASSES.items()}


class MalformedSomeip(ValueError):
    """Raised when a datagram is not a valid SOME/IP or SOME/IP-SD message."""


def _body_of(pkt: SOMEIP) -> bytes:
    # SOMEIP keeps its body in the ``data`` field. ``pkt.payload`` holds the bytes after the message.
    return b"".join(d.load for d in pkt.data if hasattr(d, "load"))


class SomeipResponse:
    """A decoded SOME/IP message.

    The constructor does not check lengths. It raises ``ValueError`` when the message type or return code is not a
    ``MessageType``/``ReturnCode`` member.
    Use ``parse_someip`` to reject malformed input.
    """

    def __init__(self, raw: bytes) -> None:
        pkt = SOMEIP(raw)
        self.service_id = pkt.srv_id
        self.method_id = pkt.sub_id
        self.client_id = pkt.client_id
        self.session_id = pkt.session_id
        self.protocol_version = pkt.proto_ver
        self.interface_version = pkt.iface_ver
        self.length = pkt.len  # The wire value, not recomputed from the payload.
        self.message_type = MessageType(int(pkt.msg_type))
        self.return_code = ReturnCode(int(pkt.retcode))
        self.payload = _body_of(pkt)


def parse_someip(data: bytes) -> SomeipResponse:
    """Decode the first SOME/IP message in *data*. Ignore the bytes after its Length.

    Raises ``MalformedSomeip`` if *data* is shorter than a header or than the Length field claims, if Length is
    below 8, if the message type is not a ``MessageType`` member, or if the return code is above 0x0A.
    It does not check the protocol and interface versions.
    """
    if len(data) < _HEADER_SIZE:
        raise MalformedSomeip(f"SOME/IP message needs {_HEADER_SIZE} bytes, got {len(data)}")
    (length,) = struct.unpack_from("!I", data, 4)
    if length < 8:
        raise MalformedSomeip(f"SOME/IP Length must be at least 8, got {length}")
    if len(data) < 8 + length:
        raise MalformedSomeip(f"SOME/IP Length {length} exceeds the {len(data) - 8} bytes that follow it")
    if data[14] not in _PARSE_MESSAGE_TYPES:
        raise MalformedSomeip(f"unknown SOME/IP message type 0x{data[14]:02x}")
    if data[15] not in _PARSE_RETURN_CODES:
        raise MalformedSomeip(f"unknown SOME/IP return code 0x{data[15]:02x}")
    return SomeipResponse(data[: 8 + length])


def build_someip(
    service_id: int,
    method_id: int,
    client_id: int,
    session_id: int,
    interface_version: int,
    message_type: int,
    return_code: int = ReturnCode.E_OK,
    payload: bytes = b"",
    protocol_version: int = 0x01,
) -> bytes:
    """Build one SOME/IP message. The Length field comes from *payload*."""
    header = SOMEIP(
        srv_id=service_id,
        sub_id=method_id,
        client_id=client_id,
        session_id=session_id,
        proto_ver=protocol_version,
        iface_ver=interface_version,
        msg_type=message_type,
        retcode=return_code,
    )
    # Scapy does not count a body in the SOMEIP ``data`` field in the Length. It counts a Raw layer.
    return bytes(header / Raw(payload)) if payload else bytes(header)


#: The Message Type bit that marks a SOME/IP-TP segment.
TP_FLAG = 0x20

#: Payload bytes of each segment except the last. A segment that is not the last has a multiple of 16 bytes.
_TP_SEGMENT_PAYLOAD = 16


def build_tp_segments(message: bytes) -> list:
    """Split the full SOME/IP *message* into the SOME/IP-TP segments that a sender sends for it.

    Each segment has the message header with a changed TP flag and Length.
    A TP header follows, with the segment offset and the More Segments flag.
    """
    header, payload = message[:16], message[16:]
    segments = []
    for offset in range(0, len(payload), _TP_SEGMENT_PAYLOAD):
        chunk = payload[offset : offset + _TP_SEGMENT_PAYLOAD]
        more_segments = (offset + len(chunk)) < len(payload)
        segments.append(
            header[:4]
            + struct.pack(">I", 8 + 4 + len(chunk))
            + header[8:14]
            + bytes((header[14] | TP_FLAG, header[15]))
            + struct.pack(">I", offset | int(more_segments))
            + chunk
        )
    return segments


@dataclasses.dataclass(frozen=True)
class SdEntry:
    """A SOME/IP-SD entry with its options resolved. Use it for received and sent entries.

    On a received entry, the index and count fields hold the wire values.
    ``options_1``/``options_2`` hold the options that these fields reference.
    To send an entry, give ``options_1``/``options_2``. ``build_sd`` sets the index and count fields.
    """

    sd_type: SdEntryType
    service_id: int
    instance_id: int
    major_version: int
    ttl: int
    minver_or_counter: int
    options_1: Tuple[Packet, ...] = ()
    options_2: Tuple[Packet, ...] = ()
    option_index_1: Optional[int] = None
    option_index_2: Optional[int] = None
    num_options_1: Optional[int] = None
    num_options_2: Optional[int] = None

    @property
    def service_minor_version(self) -> int:
        """Minor version of a FindService or OfferService entry. Raises ``TypeError`` for other types."""
        self._require(_SERVICE_ENTRY_TYPES, "service_minor_version")
        return self.minver_or_counter

    @property
    def eventgroup_counter(self) -> int:
        """Counter of a Subscribe or SubscribeAck entry. Raises ``TypeError`` for other types."""
        self._require(_EVENTGROUP_ENTRY_TYPES, "eventgroup_counter")
        return (self.minver_or_counter >> 16) & 0x0F

    @property
    def eventgroup_id(self) -> int:
        """Eventgroup ID of a Subscribe or SubscribeAck entry. Raises ``TypeError`` for other types."""
        self._require(_EVENTGROUP_ENTRY_TYPES, "eventgroup_id")
        return self.minver_or_counter & 0xFFFF

    def _require(self, types: Tuple[SdEntryType, ...], name: str) -> None:
        if self.sd_type not in types:
            raise TypeError(f"SD entry of type {self.sd_type.name} has no {name}")


@dataclasses.dataclass(frozen=True)
class SdMessage:
    """A decoded SOME/IP-SD payload. The header fields hold the raw wire values."""

    flag_reboot: bool
    flag_unicast: bool
    flags_unknown: int  # Flags bits 5-0, reserved.
    reserved: int  # The 24-bit Reserved field after Flags.
    entries_length: int
    options_length: int
    entries: Tuple[SdEntry, ...]


def _parse_option(raw: bytes) -> Packet:
    cls = _OPTION_CLASSES.get(raw[2])
    if cls is None or len(raw) < _OPTION_MIN_SIZE[raw[2]]:
        return Raw(raw)
    return cls(raw)


def _split_options(options: bytes) -> list:
    parsed = []
    offset = 0
    while offset < len(options):
        if len(options) - offset < _SD_OPTION_HEADER_SIZE:
            raise MalformedSomeip(f"SD option header at offset {offset} is cut off by the Options Array end")
        (length,) = struct.unpack_from("!H", options, offset)
        end = offset + _SD_OPTION_HEADER_SIZE + length
        if end > len(options):
            raise MalformedSomeip(f"SD option at offset {offset} with Length {length} ends past the Options Array")
        parsed.append(_parse_option(options[offset:end]))
        offset = end
    return parsed


def _parse_entry(raw: bytes, options: list) -> SdEntry:
    try:
        entry_type = SdEntryType(raw[0])
    except ValueError as exc:
        raise MalformedSomeip(f"unknown SD entry type 0x{raw[0]:02x}") from exc
    if entry_type in _SERVICE_ENTRY_TYPES:
        pkt = SDEntry_Service(raw)
        minver_or_counter = pkt.minor_ver
    else:
        pkt = SDEntry_EventGroup(raw)
        minver_or_counter = (pkt.res << 20) | (pkt.cnt << 16) | pkt.eventgroup_id
    runs = ((pkt.index_1, pkt.n_opt_1), (pkt.index_2, pkt.n_opt_2))
    for run, (index, count) in enumerate(runs, start=1):
        # An empty run references no option, so the code does not check its index.
        if count and index + count > len(options):
            raise MalformedSomeip(f"SD entry option run {run} ({index}:{index + count}) exceeds {len(options)} options")
    return SdEntry(
        sd_type=entry_type,
        service_id=pkt.srv_id,
        instance_id=pkt.inst_id,
        major_version=pkt.major_ver,
        ttl=pkt.ttl,
        minver_or_counter=minver_or_counter,
        options_1=tuple(options[pkt.index_1 : pkt.index_1 + pkt.n_opt_1]),
        options_2=tuple(options[pkt.index_2 : pkt.index_2 + pkt.n_opt_2]),
        option_index_1=pkt.index_1,
        option_index_2=pkt.index_2,
        num_options_1=pkt.n_opt_1,
        num_options_2=pkt.n_opt_2,
    )


def parse_sd(data: bytes) -> Tuple[SomeipResponse, SdMessage]:
    """Decode a SOME/IP-SD datagram and resolve the options of each entry.

    Raises ``MalformedSomeip`` on everything ``parse_someip`` rejects, and when the SD payload is shorter than 12 bytes,
    an array Length exceeds the payload, the Entries Array is not a whole number of entries, an option overruns the
    Options Array, an entry type is unknown, or a non-empty option run points past the Options Array.
    It does not check the message ID. ``parse_sd_datagram`` also checks the message ID.
    """
    msg = parse_someip(data)
    body = msg.payload
    if len(body) < _SD_HEADER_SIZE:
        raise MalformedSomeip(f"SD payload needs {_SD_HEADER_SIZE} bytes, got {len(body)}")
    flags = body[0]
    (entries_length,) = struct.unpack_from("!I", body, 4)
    options_length_at = 8 + entries_length
    if len(body) < options_length_at + 4:
        raise MalformedSomeip(f"SD Entries Array Length {entries_length} exceeds the payload")
    if entries_length % _SD_ENTRY_SIZE:
        raise MalformedSomeip(f"SD Entries Array Length {entries_length} is not a multiple of {_SD_ENTRY_SIZE}")
    (options_length,) = struct.unpack_from("!I", body, options_length_at)
    options_at = options_length_at + 4
    if len(body) < options_at + options_length:
        raise MalformedSomeip(f"SD Options Array Length {options_length} exceeds the payload")

    options = _split_options(body[options_at : options_at + options_length])
    entries = tuple(
        _parse_entry(body[offset : offset + _SD_ENTRY_SIZE], options)
        for offset in range(8, options_length_at, _SD_ENTRY_SIZE)
    )
    sd = SdMessage(
        flag_reboot=bool(flags & _SD_FLAG_REBOOT),
        flag_unicast=bool(flags & _SD_FLAG_UNICAST),
        flags_unknown=flags & ~(_SD_FLAG_REBOOT | _SD_FLAG_UNICAST) & 0xFF,
        reserved=int.from_bytes(body[1:4], "big"),
        entries_length=entries_length,
        options_length=options_length,
        entries=entries,
    )
    return msg, sd


def parse_sd_datagram(data: bytes) -> Optional[Tuple[SomeipResponse, SdMessage]]:
    """Decode *data* if it is a valid SOME/IP-SD datagram with the SD message ID. Otherwise, return ``None``."""
    try:
        msg, sd = parse_sd(data)
    except MalformedSomeip:
        return None
    if msg.service_id != SD_SERVICE_ID or msg.method_id != SD_METHOD_ID:
        return None
    return msg, sd


def _assign_run(run: Tuple[Packet, ...], options: list) -> Tuple[int, int]:
    if not run:
        return 0, 0
    if len(run) > _MAX_RUN_LENGTH:
        raise ValueError(f"an SD option run holds at most {_MAX_RUN_LENGTH} options, got {len(run)}")
    wanted = [bytes(o) for o in run]
    present = [bytes(o) for o in options]
    for start in range(len(present) - len(wanted) + 1):
        if present[start : start + len(wanted)] == wanted:
            return start, len(wanted)
    if len(present) > _MAX_RUN_INDEX:
        raise ValueError(f"an SD option run starts at index {_MAX_RUN_INDEX} at most, got {len(present)}")
    options.extend(run)
    return len(present), len(wanted)


def _entry_packet(entry: SdEntry, options: list) -> Packet:
    # Scapy cuts a TTL or eventgroup word that is too large to its field width. It does not raise an error.
    if not 0 <= entry.ttl <= _MAX_TTL:
        raise ValueError(f"SD entry TTL 0x{entry.ttl:x} does not fit in 24 bits")
    if not 0 <= entry.minver_or_counter <= 0xFFFFFFFF:
        raise ValueError(f"SD entry minver_or_counter 0x{entry.minver_or_counter:x} does not fit in 32 bits")
    index_1, count_1 = _assign_run(entry.options_1, options)
    index_2, count_2 = _assign_run(entry.options_2, options)
    common = dict(
        type=int(entry.sd_type),
        index_1=index_1,
        index_2=index_2,
        n_opt_1=count_1,
        n_opt_2=count_2,
        srv_id=entry.service_id,
        inst_id=entry.instance_id,
        major_ver=entry.major_version,
        ttl=entry.ttl,
    )
    if entry.sd_type in _SERVICE_ENTRY_TYPES:
        return SDEntry_Service(minor_ver=entry.minver_or_counter, **common)
    value = entry.minver_or_counter
    return SDEntry_EventGroup(res=value >> 20, cnt=(value >> 16) & 0x0F, eventgroup_id=value & 0xFFFF, **common)


def build_sd(
    entries: Sequence[SdEntry],
    *,
    session_id: int,
    flag_unicast: bool = True,
    flag_reboot: bool = False,
) -> bytes:
    """Build one SOME/IP-SD datagram with *entries* in the order given.

    The ``options_1``/``options_2`` of all entries go into one shared Options Array.
    When a run is already in the array, the entry references it and the code does not add it again.
    Raises ``ValueError`` for an option run longer than 15 or starting past index 255, a TTL wider than 24 bits, or a
    ``minver_or_counter`` wider than 32 bits.
    """
    options: list = []
    entry_packets = [_entry_packet(entry, options) for entry in entries]
    flags = (_SD_FLAG_REBOOT if flag_reboot else 0) | (_SD_FLAG_UNICAST if flag_unicast else 0)
    sd = SD(flags=flags, entry_array=entry_packets, option_array=options)
    return build_someip(
        SD_SERVICE_ID,
        SD_METHOD_ID,
        _SD_CLIENT_ID,
        session_id,
        SD_INTERFACE_VERSION,
        MessageType.NOTIFICATION,
        payload=bytes(sd),
    )
