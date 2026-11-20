#!/usr/bin/env python3
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

"""SOME/IP send-path latency analyzer: ``latency = T_wire - T_app``, one-way, on the sender.

The sender stamps T_app immediately before Send(). T_wire is the egress of the matching frame in a sender-side
``tcpdump`` capture. The two are in the ``CLOCK_REALTIME`` domain.
First, the analyzer reassembles the frames (SOME/IP-TP over UDP, the byte stream per flow over TCP).
The egress time of a message is the time of its last segment.
The pairing is per ``(event_id, payload_len, dst_port)``, by the counter that the sender writes into its payload.
Thus a lost frame gives exactly one send without a pair.
The module needs only the standard library and ``pcap_reader``, thus it runs on all analysis hosts.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from pcap_reader import Packet, build_tcp_frame, build_udp_frame, read_pcap, streams, write_pcap


SOMEIP_HEADER_LEN = 16  # bytes, fixed
SOMEIP_TP_HEADER_LEN = 4  # TP sub-header that precedes the segment payload
MSGTYPE_TP_FLAG = 0x20  # bit in the message type of segmented messages
MSGTYPE_NOTIFICATION = 0x02  # events are NOTIFICATION (0x02), TP -> 0x22
EVENT_ID_NOTIFICATION_BIT = 0x8000  # high bit of the method ID of events on the wire

# The location of the sender counter in each payload type: ``(payload offset, struct format)``.
# The key is the type name of the event in the event map.
# The sender writes ``static_cast<T>(counter)`` for a primitive and ``counter & 1`` for Bool.
# For Struct, it writes the low byte into the uint8 after the float.
# It writes the big-endian counter into the first data bytes of Bytes (after its 4-byte length field), of FixedArray
# and of the NestedStruct padding. That padding is after a uint32, a uint64 and 43 bytes of inner scalars.
# A signed type uses two's complement. Thus its bytes are the same as in the unsigned format.
COUNTER_FIELDS: dict[str, tuple[int, str]] = {
    "Uint8": (0, "B"),
    "Int8": (0, "B"),
    "Uint16": (0, "H"),
    "Int16": (0, "H"),
    "Uint32": (0, "I"),
    "Int32": (0, "I"),
    "Uint64": (0, "Q"),
    "Int64": (0, "Q"),
    "Float": (0, "f"),
    "Double": (0, "d"),
    "Bool": (0, "?"),
    "Struct": (4, "B"),
    "Bytes": (4, "I"),
    "FixedArray": (0, "I"),
    "NestedStruct": (55, "I"),
}

# The payload bytes that a frame keeps. This is sufficient for the counter field with the largest offset.
HEAD_LEN = max(offset + struct.calcsize(fmt) for offset, fmt in COUNTER_FIELDS.values())


def expected_bytes(fmt: str, counter: int) -> set[bytes]:
    """Return the wire bytes of *counter* in a field of struct format *fmt*, in the two byte orders.

    The byte order of the field is the serializer byte order. It is different between SOME/IP stacks and is not
    part of this contract. Thus the big-endian and the little-endian encodings are both a match.
    The big-endian bytes of one counter are equal to the little-endian bytes of a different counter only if the two
    counters differ by 255 or more. Thus a collision needs that many pending sends in the candidate window.
    """
    if fmt == "?":
        value: int | float | bool = bool(counter & 1)
    elif fmt in "fd":
        value = float(counter)
    else:
        value = counter & ((1 << (8 * struct.calcsize(fmt))) - 1)
    return {struct.pack(">" + fmt, value), struct.pack("<" + fmt, value)}


@dataclass
class SomeIpSegment:
    """One SOME/IP message in one transport datagram or segment."""

    ts: int  # integer nanoseconds (ts of the FIRST byte of this message)
    end_ts: int  # integer nanoseconds (ts of the LAST byte of this message)
    service_id: int
    method_id: int  # raw method/event ID field (an event has the high bit set)
    length: int  # SOME/IP length field (from the client ID to the end)
    client_id: int
    session_id: int
    proto_version: int
    iface_version: int
    msg_type: int
    return_code: int
    is_tp: bool
    tp_offset: int  # byte offset of this segment within the full payload
    tp_more: bool  # more-segments flag
    payload: bytes  # the payload bytes of this segment
    dst_ip: str = ""  # transport destination (the subscriber endpoint)
    dst_port: int = 0  # unicast fan-out: one dst port for each subscriber

    @property
    def event_id(self) -> int:
        return self.method_id & ~EVENT_ID_NOTIFICATION_BIT


def _plausible_header(buf: bytes, pos: int) -> bool:
    """True if a notification header can start at *pos*: event bit, protocol version 1, notification type."""
    if pos + SOMEIP_HEADER_LEN > len(buf):
        return False
    (msg_id, length, _request_id, pv, _iv, mt, _rc) = struct.unpack(">IIIBBBB", buf[pos : pos + SOMEIP_HEADER_LEN])
    return (
        bool(msg_id & EVENT_ID_NOTIFICATION_BIT)
        and length >= 8
        and pv == 1
        and (mt & ~MSGTYPE_TP_FLAG) == MSGTYPE_NOTIFICATION
    )


def _next_header(buf: bytes, start: int) -> int:
    """Return the offset of the first plausible notification header at or after *start*, else ``len(buf)``."""
    pos = start
    while pos + SOMEIP_HEADER_LEN <= len(buf) and not _plausible_header(buf, pos):
        pos += 1
    return pos


def _parse_someip_buffer(buf: bytes, ts_at, dst=("", 0), resync: bool = False) -> list[SomeIpSegment]:
    """Parse the consecutive SOME/IP messages in *buf*. Stop at the first incomplete message at the end.

    *ts_at* maps a byte offset to its capture timestamp. It is constant for a UDP datagram and per segment for TCP.
    Each message keeps the times of its first and last byte as ``ts`` and ``end_ts``.
    *dst* is the ``(ip, port)`` transport destination. It separates the egress per client for unicast fan-out.
    *resync* is for a buffer that can start in a message, after a capture gap. Then the parse starts at the first
    plausible notification header. Thus the message at the gap is lost, and the messages after it are kept.
    """
    out: list[SomeIpSegment] = []
    pos = _next_header(buf, 0) if resync else 0
    while pos + SOMEIP_HEADER_LEN <= len(buf):
        (msg_id, length, request_id, pv, iv, mt, rc) = struct.unpack(">IIIBBBB", buf[pos : pos + SOMEIP_HEADER_LEN])
        # length is request-id(4)+pv/iv/mt/rc(4)+payload. Total message = 8 + length
        total = 8 + length
        if length < 8 or pos + total > len(buf):
            break
        body = buf[pos + SOMEIP_HEADER_LEN : pos + total]
        is_tp = bool(mt & MSGTYPE_TP_FLAG)
        tp_offset, tp_more, payload = 0, False, body
        if is_tp and len(body) >= SOMEIP_TP_HEADER_LEN:
            (tp_field,) = struct.unpack(">I", body[:SOMEIP_TP_HEADER_LEN])
            # The TP field has the byte offset in its top 28 bits and the more-segments flag in bit 0.
            # Thus a mask of the low nibble gives the offset. It is always a multiple of 16.
            tp_offset = tp_field & 0xFFFFFFF0
            tp_more = bool(tp_field & 0x1)
            payload = body[SOMEIP_TP_HEADER_LEN:]
        out.append(
            SomeIpSegment(
                ts=ts_at(pos),
                end_ts=ts_at(pos + total - 1),
                service_id=(msg_id >> 16) & 0xFFFF,
                method_id=msg_id & 0xFFFF,
                length=length,
                client_id=(request_id >> 16) & 0xFFFF,
                session_id=request_id & 0xFFFF,
                proto_version=pv,
                iface_version=iv,
                msg_type=mt,
                return_code=rc,
                is_tp=is_tp,
                tp_offset=tp_offset,
                tp_more=tp_more,
                payload=payload,
                dst_ip=dst[0],
                dst_port=dst[1],
            )
        )
        pos += total
    return out


@dataclass
class Frame:
    """A logical event notification, reassembled from one or more segments."""

    service_id: int
    event_id: int
    session_id: int
    first_ts: int  # first segment timestamp, ns
    last_ts: int  # last segment timestamp, ns. All of the message is on the wire, thus this is T_wire
    payload_len: int  # total reassembled payload length
    dst_ip: str = ""  # subscriber endpoint of this notification
    dst_port: int = 0  # different for each subscriber with unicast fan-out
    head: bytes = b""  # the first HEAD_LEN payload bytes, which contain the sender counter


def extract_segments(packets: list[Packet]) -> list[SomeIpSegment]:
    """Make SOME/IP segments from captured packets. Parse each UDP datagram or contiguous run of a TCP direction."""
    segments: list[SomeIpSegment] = []
    for stream in streams(packets):
        # A capture gap splits the direction into runs. A run after a gap starts in a message.
        # Thus the parser searches it for its first header.
        bounds = [0, *stream.gaps, len(stream.data)]
        for run, (start, end) in enumerate(zip(bounds, bounds[1:])):
            segments.extend(
                _parse_someip_buffer(
                    stream.data[start:end],
                    lambda pos, start=start, ts_at=stream.ts_at: ts_at(start + pos),
                    # flow = (src, dst, sport, dport). The subscriber is the destination.
                    dst=(stream.flow[1], stream.flow[3]),
                    resync=run > 0,
                )
            )
    return segments


def reassemble_frames(packets: list[Packet]) -> list[Frame]:
    """Group SOME/IP NOTIFICATION segments into logical frames.

    A message without TP is one frame.
    Only UDP has TP messages. The function groups them by (service, event, session) until the more-segments flag is 0.
    Thus their timestamps include all segments, and the payload length is the sum.
    A TCP message is already reassembled into one segment.
    """
    # The destination port is part of the key. With unicast fan-out, the same (service, event, session)
    # goes to each subscriber. Without the port, the copies become one frame.
    tp: dict[tuple[int, int, int, int], dict] = {}
    frames: list[Frame] = []
    for seg in extract_segments(packets):
        base_type = seg.msg_type & ~MSGTYPE_TP_FLAG
        if base_type != MSGTYPE_NOTIFICATION:
            continue
        key = (seg.service_id, seg.event_id, seg.session_id, seg.dst_port)
        if not seg.is_tp:
            frames.append(
                Frame(
                    service_id=seg.service_id,
                    event_id=seg.event_id,
                    session_id=seg.session_id,
                    first_ts=seg.ts,
                    last_ts=seg.end_ts,
                    payload_len=len(seg.payload),
                    dst_ip=seg.dst_ip,
                    dst_port=seg.dst_port,
                    head=seg.payload[:HEAD_LEN],
                )
            )
            continue
        acc = tp.get(key)
        if acc is None:
            acc = {"first_ts": seg.ts, "last_ts": seg.end_ts, "segments": {}, "head": bytearray(HEAD_LEN)}
            tp[key] = acc
        acc["first_ts"] = min(acc["first_ts"], seg.ts)
        acc["last_ts"] = max(acc["last_ts"], seg.end_ts)
        acc["segments"][seg.tp_offset] = len(seg.payload)
        # All segments that overlap the head make the head. Thus a short first segment cannot truncate a field.
        if seg.tp_offset < HEAD_LEN:
            chunk = seg.payload[: HEAD_LEN - seg.tp_offset]
            acc["head"][seg.tp_offset : seg.tp_offset + len(chunk)] = chunk
        if not seg.tp_more:
            total = sum(acc["segments"].values())
            frames.append(
                Frame(
                    service_id=seg.service_id,
                    event_id=seg.event_id,
                    session_id=seg.session_id,
                    first_ts=acc["first_ts"],
                    last_ts=acc["last_ts"],
                    payload_len=total,
                    dst_ip=seg.dst_ip,
                    dst_port=seg.dst_port,
                    head=bytes(acc["head"]),
                )
            )
            del tp[key]
    return frames


@dataclass
class AppSend:
    """One Send() that the sender app recorded, with T_app (CLOCK_REALTIME)."""

    event_id: int
    size: int
    counter: int | None  # the sender counter, which is also in the payload. None if the log has no counter
    t_app_ns: int  # integer nanoseconds (CLOCK_REALTIME)


def _parse_int(value: str) -> int:
    value = value.strip()
    return int(value, 0) if value.lower().startswith("0x") else int(value)


def load_app_log(path: str) -> list[AppSend]:
    """Load the send-log CSV of the sender. Its columns are ``event_id,size,counter,t_app_ns``.

    ``counter`` is the value that the sender wrote into the payload of that send. An empty value loads as None.
    ``t_app_ns`` is a CLOCK_REALTIME nanosecond timestamp, taken immediately before Send().
    The function silently skips a row with a missing column or a value that it cannot parse.
    """
    import csv

    out: list[AppSend] = []
    with open(path, newline="") as fh:
        for row in csv.DictReader(fh):
            try:
                event_id = _parse_int(row["event_id"])
                size = int(row["size"])
                counter_text = (row.get("counter") or "").strip()
                counter = _parse_int(counter_text) if counter_text else None
                t_app_ns = int(row["t_app_ns"])
            except (KeyError, ValueError, TypeError):
                # DictReader sets the missing columns of a short row to None. int() raises TypeError for None.
                continue
            out.append(AppSend(event_id=event_id, size=size, counter=counter, t_app_ns=t_app_ns))
    return out


@dataclass
class Paired:
    """One send matched to its frame. This is one latency sample."""

    service_id: int
    event_id: int
    payload_len: int
    dst_port: int  # subscriber endpoint of this egress frame
    latency_ns: int  # integer nanoseconds, T_wire - T_app
    egress_ts: int  # T_wire, integer ns (frame.last_ts), for the rate


def _carries(frame: Frame, send: AppSend, field: tuple[int, str] | None) -> bool:
    """True if the payload of *frame* contains the counter of *send*.

    Also true without a field or a counter, and for a payload that is too short for the field.
    The sender does not write a counter into such a payload.
    """
    if field is None or send.counter is None:
        return True
    offset, fmt = field
    stamped = frame.head[offset : offset + struct.calcsize(fmt)]
    return len(stamped) < struct.calcsize(fmt) or stamped in expected_bytes(fmt, send.counter)


def _latest_match(sends: list[AppSend], cursor: int, frame: Frame, field: tuple[int, str] | None) -> int | None:
    """Return the index of the latest send, from *cursor*, that can be in *frame*, or None.

    A send with a stamp after the first byte of the frame cannot be in it. The sends are in stamp order.
    Thus the scan stops at the first such send.
    """
    match = None
    for j in range(cursor, len(sends)):
        if sends[j].t_app_ns > frame.first_ts:
            break
        if _carries(frame, sends[j], field):
            match = j
    return match


def pair_app_log(
    frames: list[Frame],
    sends: list[AppSend],
    known_ports: list[int] | None = None,
    counter_fields: dict[int, tuple[int, str]] | None = None,
) -> tuple[list[Paired], int, dict[int, int]]:
    """Pair egress frames to app sends per ``(event_id, payload_len, dst_port)``. Return pairs and drop counts.

    *counter_fields* maps an event ID to the location of the send counter in its payload, as in ``COUNTER_FIELDS``.
    A frame pairs only with a send that has its counter and a stamp that is not after its first byte.
    Thus a lost frame gives exactly one send without a pair, and the function ignores a frame without a send.
    An event without a counter field, or a send without a counter, pairs only by order and time.
    Bool has only one bit. Thus its pairing is correct only while the latency is less than the send gap.
    With unicast fan-out, each port has its own pairing of the sends. If given, *known_ports* is the correct
    subscriber set. Thus a silent subscriber shows all sends as dropped, and unknown ports are not included.
    """
    counter_fields = counter_fields or {}
    frame_buckets: dict[tuple[int, int, int], list[Frame]] = defaultdict(list)
    observed = set()
    for fr in frames:
        frame_buckets[(fr.event_id, fr.payload_len, fr.dst_port)].append(fr)
        observed.add(fr.dst_port)
    for v in frame_buckets.values():
        v.sort(key=lambda fr: fr.last_ts)

    send_buckets: dict[tuple[int, int], list[AppSend]] = defaultdict(list)
    for s in sends:
        send_buckets[(s.event_id, s.size)].append(s)
    for v in send_buckets.values():
        v.sort(key=lambda s: s.t_app_ns)

    # Without ports, all goes into one port-0 bucket. Thus a capture without notifications shows each send
    # as dropped, and does not report nothing.
    ports = sorted(known_ports) if known_ports else sorted(observed) if observed else [0]

    paired: list[Paired] = []
    dropped = 0
    dropped_by_port: dict[int, int] = dict.fromkeys(ports, 0)
    for (event_id, size), bucket_sends in send_buckets.items():
        field = counter_fields.get(event_id)
        for port in ports:
            cursor = 0
            paired_here = 0
            # Use the latest candidate. Thus the skipped sends count as drops.
            # A frame without a candidate does not move the cursor.
            for fr in frame_buckets.get((event_id, size, port), []):
                match = _latest_match(bucket_sends, cursor, fr, field)
                if match is None:
                    continue
                paired.append(
                    Paired(
                        service_id=0,
                        event_id=event_id,
                        payload_len=size,
                        dst_port=port,
                        latency_ns=fr.last_ts - bucket_sends[match].t_app_ns,
                        egress_ts=fr.last_ts,
                    )
                )
                paired_here += 1
                cursor = match + 1
            missed = len(bucket_sends) - paired_here
            dropped += missed
            dropped_by_port[port] += missed
    return paired, dropped, dropped_by_port


def _percentile(sorted_vals: list[float], pct: float) -> float:
    """Return the linear-interpolated percentile of a sorted list, or NaN if the list is empty.

    The result is an interpolation between adjacent samples, not one of the samples.
    Thus a p99 of less than 100 samples is a calculated value that no send had.
    """
    if not sorted_vals:
        return float("nan")
    if len(sorted_vals) == 1:
        return sorted_vals[0]
    rank = pct / 100.0 * (len(sorted_vals) - 1)
    lo = int(rank)
    hi = min(lo + 1, len(sorted_vals) - 1)
    frac = rank - lo
    return sorted_vals[lo] * (1 - frac) + sorted_vals[hi] * frac


@dataclass
class Stats:
    """One table row: the latency distribution of one (event, size, subscriber) bucket, in microseconds.

    ``dropped`` has the full loss count of the subscriber. Only the first row of that subscriber has a value that is
    not zero.
    """

    label: str
    transport: str
    client: str  # the subscriber label, or ``:<port>``
    event_id: int
    size: int
    count: int  # paired samples, not sends
    dropped: int
    pps: float  # measured egress rate in this bucket (packets/second)
    mn: float
    mean: float
    p50: float
    p90: float
    p99: float
    mx: float


def aggregate(
    paired: list[Paired],
    dropped_by_port: dict[int, int],
    event_map: dict[int, dict],
    client_ports: dict[int, str] | None = None,
) -> list[Stats]:
    """Group paired latencies by (event_id, payload_len, dst_port) into statistics per bucket, in microseconds.

    Each destination port is a different subscriber. *client_ports* maps a port to a label.
    """
    client_ports = client_ports or {}
    groups: dict[tuple[int, int, int], list[int]] = {}
    egress: dict[tuple[int, int, int], list[int]] = {}
    for p in paired:
        key = (p.event_id, p.payload_len, p.dst_port)
        groups.setdefault(key, []).append(p.latency_ns)
        egress.setdefault(key, []).append(p.egress_ts)
    out: list[Stats] = []
    for (event_id, size, port), vals in sorted(groups.items()):
        us = sorted(v / 1000.0 for v in vals)  # nanoseconds -> microseconds
        meta = event_map.get(event_id, {})
        # There are count-1 intervals between the first and the last egress.
        # Thus the rate is count-1 divided by that time, not count divided by it.
        ts = egress[(event_id, size, port)]
        span_ns = max(ts) - min(ts)
        pps = (len(ts) - 1) / (span_ns / 1e9) if len(ts) >= 2 and span_ns > 0 else 0.0
        out.append(
            Stats(
                label=meta.get("type", f"event_{event_id:#x}"),
                transport=meta.get("transport", "?"),
                client=client_ports.get(port, f":{port}"),
                event_id=event_id,
                size=size,
                count=len(us),
                dropped=0,
                pps=pps,
                mn=us[0],
                mean=sum(us) / len(us),
                p50=_percentile(us, 50),
                p90=_percentile(us, 90),
                p99=_percentile(us, 99),
                mx=us[-1],
            )
        )
    # The full loss of each subscriber goes on its first row. Thus the table does not repeat it for each bucket.
    seen_ports = set()
    for s, (_event_id, _size, port) in zip(out, sorted(groups), strict=True):
        if port not in seen_ports:
            s.dropped = dropped_by_port.get(port, 0)
            seen_ports.add(port)
    return out


def render_table(stats: list[Stats]) -> str:
    hdr = (
        f"{'type':<16}{'tport':<6}{'client':<8}{'size':>7}{'count':>7}{'pps':>10}"
        f"{'min':>10}{'mean':>10}{'p50':>10}{'p90':>10}{'p99':>10}{'max':>10}"
        "  (pps=pkt/s, latency in us)"
    )
    lines = [hdr, "-" * len(hdr)]
    for s in stats:
        lines.append(
            f"{s.label:<16}{s.transport:<6}{s.client:<8}{s.size:>7}{s.count:>7}"
            f"{s.pps:>10.1f}{s.mn:>10.2f}{s.mean:>10.2f}{s.p50:>10.2f}{s.p90:>10.2f}"
            f"{s.p99:>10.2f}{s.mx:>10.2f}"
        )
    total_dropped = sum(s.dropped for s in stats)
    lines.append("-" * len(hdr))
    lines.append(f"dropped ((send, subscriber) pairs with no egress frame): {total_dropped}")
    return "\n".join(lines)


def write_csv(stats: list[Stats], path: str) -> None:
    """Write one row for each :class:`Stats` bucket. The header below is the column contract."""
    import csv

    with open(path, "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(
            [
                "type",
                "transport",
                "client",
                "event_id",
                "size_bytes",
                "count",
                "pps",
                "min_us",
                "mean_us",
                "p50_us",
                "p90_us",
                "p99_us",
                "max_us",
            ]
        )
        for s in stats:
            w.writerow(
                [
                    s.label,
                    s.transport,
                    s.client,
                    s.event_id,
                    s.size,
                    s.count,
                    f"{s.pps:.1f}",
                    f"{s.mn:.3f}",
                    f"{s.mean:.3f}",
                    f"{s.p50:.3f}",
                    f"{s.p90:.3f}",
                    f"{s.p99:.3f}",
                    f"{s.mx:.3f}",
                ]
            )


def load_event_map(path: str | None) -> dict[int, dict]:
    """Load an event ID to ``{type, transport}`` map. The keys can be decimal or hex strings.

    The function skips other keys, for example a comment entry.
    """
    if not path:
        return {}
    with open(path) as fh:
        raw = json.load(fh)
    out: dict[int, dict] = {}
    for k, v in raw.items():
        try:
            out[int(k, 0) if isinstance(k, str) else int(k)] = v
        except (ValueError, TypeError):
            continue  # skip comment/non-numeric keys
    return out


def analyze_app_log(
    app_log_csv: str, sender_pcap: str, event_map: dict[int, dict], client_ports: dict[int, str] | None = None
) -> list[Stats]:
    """Pair the send-log of the sender with the sender-side pcap and calculate the statistics.

    The type names in *event_map* label the rows and select the counter field of each event from ``COUNTER_FIELDS``.
    A type that is not in the list pairs only by order and time. The function reports it on stderr.
    *client_ports* maps the destination port of a subscriber to a label, for example ``{60010: "c0", 60011: "c1"}``.
    Without it, the label of each client is its port.
    Each send goes out one time for each subscriber. Thus the result has one row for each ``(event, size, subscriber)``.
    """
    sends = load_app_log(app_log_csv)
    frames = reassemble_frames(read_pcap(sender_pcap))
    known_ports = list(client_ports.keys()) if client_ports else None
    counter_fields: dict[int, tuple[int, str]] = {}
    for event_id, meta in event_map.items():
        field = COUNTER_FIELDS.get(meta.get("type"))
        if field is None:
            print(
                f"wire_latency: event {event_id:#x} ({meta.get('type')}) has no counter field, "
                "so its frames pair by order and time alone",
                file=sys.stderr,
            )
        else:
            counter_fields[event_id] = field
    paired, _dropped, dropped_by_port = pair_app_log(frames, sends, known_ports, counter_fields)
    return aggregate(paired, dropped_by_port, event_map, client_ports)


def _build_someip(
    service_id, event_id, session_id, payload, *, msg_type=MSGTYPE_NOTIFICATION, tp_offset=None, tp_more=False
):
    method_id = event_id | EVENT_ID_NOTIFICATION_BIT
    body = payload
    if tp_offset is not None:
        msg_type |= MSGTYPE_TP_FLAG
        tp_field = (tp_offset & 0xFFFFFFF0) | (1 if tp_more else 0)
        body = struct.pack(">I", tp_field) + payload
    length = 8 + len(body)
    msg_id = (service_id << 16) | method_id
    request_id = (0x0001 << 16) | session_id
    return struct.pack(">IIIBBBB", msg_id, length, request_id, 0x01, 0x01, msg_type, 0x00) + body


def _ns(seconds: float) -> int:
    return int(round(seconds * 1e9))


def selftest() -> int:
    """Make a pcap and a send-log with known latencies, then check each stage with them.

    The fixtures show the analyzer contract: frames without a log row before the first send, which the analyzer
    must drop; single-packet sends with a known egress delay; a UDP-TP message with the latency of its last segment;
    a multi-client fan-out; and a TCP stream split into segments.
    Returns 0 if all checks pass, else 1.
    """
    import os
    import tempfile

    tmp = Path(tempfile.mkdtemp(prefix="wirelat_"))
    pcap_path = tmp / "sender.pcap"
    log_path = tmp / "sendlog.csv"

    SVC, EVID = 0x0103, 0x66
    pcap_frames: list[tuple[int, bytes]] = []
    log_rows: list[tuple[int, int, int | None, int]] = []  # event_id,size,counter,t_app_ns

    # Two frames of size 16 before all measured sends, without a send-log row.
    # The analyzer must drop them. If it pairs them, all later samples move.
    for i in range(2):
        stray = _build_someip(SVC, EVID, 0x0900 + i, struct.pack(">I", 900 + i) + bytes(12))
        pcap_frames.append((_ns(990.0 + i), build_udp_frame(stray)))

    # 3 single-packet sends (counters 1..3), size 16, each with egress 50 us after T_app.
    for i, ctr in enumerate((1, 2, 3)):
        t_app = 1000.0 + i
        payload = struct.pack(">II", 12, ctr) + bytes(8)  # 16-byte Bytes payload: length field, counter, padding
        msg = _build_someip(SVC, EVID, 0x1000 + i, payload)
        pcap_frames.append((_ns(t_app + 50e-6), build_udp_frame(msg)))  # egress 50us later
        log_rows.append((EVID, 16, ctr, int(round(t_app * 1e9))))

    # 1 lost send (in the app log, but no egress): counter 4, size 16.
    log_rows.append((EVID, 16, 4, int(round(1100.0 * 1e9))))

    # 1 TP send (counter 5, 2 segments of 32 bytes, 64-byte payload).
    t_app_tp = 1200.0
    seg0 = struct.pack(">II", 60, 5) + bytes(24)  # 32 bytes, offset 0, more=1
    seg1 = bytes(32)  # 32 bytes, offset 32, more=0
    m0 = _build_someip(SVC, EVID, 0x3000, seg0, tp_offset=0, tp_more=True)
    m1 = _build_someip(SVC, EVID, 0x3000, seg1, tp_offset=32, tp_more=False)
    pcap_frames.append((_ns(t_app_tp + 150e-6), build_udp_frame(m0)))
    pcap_frames.append((_ns(t_app_tp + 200e-6), build_udp_frame(m1)))  # last segment, thus the T_wire of the frame
    log_rows.append((EVID, 64, 5, int(round(t_app_tp * 1e9))))

    write_pcap(pcap_path, pcap_frames)
    with open(log_path, "w") as fh:
        fh.write("event_id,size,counter,t_app_ns\n")
        for ev, sz, ctr, t_ns in log_rows:
            fh.write(f"{ev},{sz},{'' if ctr is None else ctr},{t_ns}\n")

    event_map = {EVID: {"type": "Bytes", "transport": "UDP"}}
    stats = analyze_app_log(log_path, pcap_path, event_map)

    ok = True

    def check(cond, msg):
        nonlocal ok
        print(("  PASS " if cond else "  FAIL ") + msg)
        ok = ok and cond

    # Two groups are expected: size 16 (3 paired, 1 dropped) and size 64 (1 TP frame).
    by_size = {s.size: s for s in stats}
    check(16 in by_size, "size-16 group present")
    check(64 in by_size, "size-64 (TP-reassembled) group present")
    if 16 in by_size:
        g = by_size[16]
        check(g.count == 3, f"size-16 count == 3 (got {g.count})")
        check(abs(g.mean - 50.0) < 0.5, f"size-16 mean ~50us (got {g.mean:.2f})")
        check(g.label == "Bytes", f"size-16 type label mapped (got {g.label})")
        # egress at 1000.00005, 1001.00005, 1002.00005 s -> span 2.0s, (3-1)/2 = 1.0 pps
        check(abs(g.pps - 1.0) < 0.01, f"size-16 pps == 1.0 (got {g.pps:.3f})")
    if 64 in by_size:
        g = by_size[64]
        check(g.count == 1, f"size-64 count == 1 (got {g.count})")
        # All of the message is on the wire only after the egress of its last TP segment.
        # Thus the expected latency is the 200 us of the last segment, not the 150 us of the first.
        check(abs(g.mean - 200.0) < 0.5, f"size-64 TP latency ~200us (got {g.mean:.2f})")
    total_dropped = sum(s.dropped for s in stats)
    check(total_dropped == 1, f"exactly 1 dropped send detected (got {total_dropped})")

    # With unicast fan-out, one send sequence goes out one time for each subscriber, one frame per destination port.
    # The analyzer must split those frames by port and pair each subscriber separately with the shared send-log.
    # This gives one row for each (event, size, client).
    mc_pcap = tmp / "multiclient.pcap"
    mc_log = tmp / "multiclient.csv"
    PORT_A, PORT_B = 60010, 60011  # two subscriber endpoints
    mc_frames: list[tuple[int, bytes]] = []
    mc_rows: list[tuple[int, int, int | None, int]] = []
    # 3 sends of size 16. Egress to client A after 50 us, to client B (more load) after 120 us.
    for i, ctr in enumerate((1, 2, 3)):
        t_app = 3000.0 + i
        payload = struct.pack(">II", 12, ctr) + bytes(8)
        msg = _build_someip(SVC, EVID, 0x5000 + i, payload)
        mc_frames.append((_ns(t_app + 50e-6), build_udp_frame(msg, dport=PORT_A)))
        mc_frames.append((_ns(t_app + 120e-6), build_udp_frame(msg, dport=PORT_B)))
        mc_rows.append((EVID, 16, ctr, int(round(t_app * 1e9))))
    # 1 send with egress only to client A (client B drops it): asymmetric loss.
    t_drop = 3100.0
    msg_d = _build_someip(SVC, EVID, 0x5900, struct.pack(">II", 12, 4) + bytes(8))
    mc_frames.append((_ns(t_drop + 50e-6), build_udp_frame(msg_d, dport=PORT_A)))
    mc_rows.append((EVID, 16, 4, int(round(t_drop * 1e9))))

    write_pcap(mc_pcap, mc_frames)
    with open(mc_log, "w") as fh:
        fh.write("event_id,size,counter,t_app_ns\n")
        for ev, sz, ctr, t_ns in mc_rows:
            fh.write(f"{ev},{sz},{'' if ctr is None else ctr},{t_ns}\n")

    mc_map = {EVID: {"type": "Bytes", "transport": "UDP"}}
    mc_stats = analyze_app_log(mc_log, mc_pcap, mc_map, client_ports={PORT_A: "c0", PORT_B: "c1"})
    by_client = {s.client: s for s in mc_stats}
    check(len(mc_stats) == 2, f"two per-client rows produced (got {len(mc_stats)})")
    check("c0" in by_client and "c1" in by_client, "both client labels present")
    if "c0" in by_client:
        a = by_client["c0"]
        check(a.count == 4, f"client c0 paired all 4 sends (got {a.count})")
        check(abs(a.mean - 50.0) < 0.5, f"client c0 mean ~50us (got {a.mean:.2f})")
        check(a.dropped == 0, f"client c0 no loss (got {a.dropped})")
    if "c1" in by_client:
        b = by_client["c1"]
        check(b.count == 3, f"client c1 paired 3 sends (got {b.count})")
        check(abs(b.mean - 120.0) < 0.5, f"client c1 mean ~120us (got {b.mean:.2f})")
        check(b.dropped == 1, f"client c1 dropped the 4th send (got {b.dropped})")
    check(
        sum(s.dropped for s in mc_stats) == 1,
        f"exactly 1 (send, subscriber) pair dropped across clients (got {sum(s.dropped for s in mc_stats)})",
    )

    # SOME/IP over TCP has no TP flag. Thus a large message is in consecutive segments.
    # One notification in two segments must become one frame that keeps the two ends: first_ts from
    # the first segment, last_ts from the last segment.
    tcp_evid, tcp_payload_len = 0x90, 3004
    big = _build_someip(SVC, tcp_evid, 0x4000, struct.pack(">I", 7) + bytes(tcp_payload_len - 4))
    isn = 0xFFFFFF00  # near the 32-bit wrap, to test the modulo
    cut = 1400  # first segment ends in the message (> MSS)
    seg_a, seg_b = big[:cut], big[cut:]
    tcp_pcap = tmp / "tcp.pcap"
    seg_a_ts, seg_b_ts = 2000.0, 2000.0 + 80e-6
    write_pcap(
        tcp_pcap,
        [
            (_ns(seg_a_ts), build_tcp_frame(seg_a, isn)),
            (_ns(seg_b_ts), build_tcp_frame(seg_b, isn + len(seg_a))),
        ],
    )
    tcp_frames = reassemble_frames(read_pcap(tcp_pcap))
    check(len(tcp_frames) == 1, f"TCP split message reassembled into 1 frame (got {len(tcp_frames)})")
    if tcp_frames:
        fr = tcp_frames[0]
        check(fr.event_id == tcp_evid, f"TCP frame event-id == {tcp_evid:#x} (got {fr.event_id:#x})")
        check(fr.payload_len == tcp_payload_len, f"TCP frame payload_len == {tcp_payload_len} (got {fr.payload_len})")
        first_ns = int(round(seg_a_ts * 1e9))
        last_ns = int(round(seg_b_ts * 1e9))
        check(
            abs(fr.first_ts - first_ns) < 1000,
            f"TCP frame first_ts == first segment ts (got {fr.first_ts}, want {first_ns})",
        )
        check(
            abs(fr.last_ts - last_ns) < 1000,
            f"TCP frame egress ts (ns) == LAST segment ts (got {fr.last_ts}, want {last_ns})",
        )

    print()
    print(render_table(stats))
    print()
    print("SELFTEST", "PASSED" if ok else "FAILED")
    return 0 if ok else 1


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--app-log", help="sender send-log CSV (event_id,size,counter,t_app_ns)")
    ap.add_argument("--pcap", help="pcap captured at the sender interface (egress)")
    ap.add_argument("--map", help="JSON event-id -> {type, transport} map")
    ap.add_argument("--csv", help="write per-(type,size) stats to this CSV path")
    ap.add_argument("--selftest", action="store_true", help="run built-in synthetic self-test and exit")
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()
    if not (args.app_log and args.pcap):
        ap.error("provide both --app-log and --pcap (or --selftest)")
    try:
        stats = analyze_app_log(args.app_log, args.pcap, load_event_map(args.map))
    except FileNotFoundError as exc:
        # A missing input means that the run did not make the capture or the send-log.
        # This is a run failure, not an analyzer failure. Thus report a message, not a traceback.
        print(
            f"wire_latency: input not found: {exc.filename} "
            "(no measurements were produced — check that the sender ran and "
            "that the capture started before emission and subscription succeeded)",
            file=sys.stderr,
        )
        return 2
    except ValueError as exc:
        print(f"wire_latency: cannot parse capture: {exc}", file=sys.stderr)
        return 2
    if not stats:
        print("wire_latency: no samples to report (0 events paired)", file=sys.stderr)
    print(render_table(stats))
    if args.csv:
        write_csv(stats, args.csv)
        print(f"\nwrote {args.csv}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
