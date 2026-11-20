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

"""Helpers that make transport byte streams from a classic pcap capture. Each byte has its capture timestamp.

A UDP datagram is one stream. One direction of a TCP connection is one stream, reassembled in sequence order.
Timestamps are integer nanoseconds of the ``CLOCK_REALTIME`` of the capture host. libpcap uses this clock.
The parser knows only Ethernet, one optional 802.1Q tag, IPv4, UDP and TCP. It skips all other frames.
The module uses only the standard library, because the analyzers that use it also run on the target.

Usage:
    pcap_reader.py --selftest
"""

from __future__ import annotations

import argparse
import bisect
import socket
import struct
import sys
import tempfile
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

ETHERTYPE_IPV4 = 0x0800
ETHERTYPE_VLAN = 0x8100
IPPROTO_TCP = 6
IPPROTO_UDP = 17

TCP_SYN = 0x02
TCP_ACK = 0x10

PCAP_MAGIC_LE = 0xA1B2C3D4  # microsecond, little-endian host
PCAP_MAGIC_BE = 0xD4C3B2A1
PCAP_MAGIC_NS_LE = 0xA1B23C4D  # nanosecond-resolution variant
PCAP_MAGIC_NS_BE = 0x4D3CB2A1

Flow = tuple[str, str, int, int]  # (src_ip, dst_ip, src_port, dst_port)
TsAt = Callable[[int], int]


@dataclass
class Packet:
    ts: int  # capture timestamp, integer nanoseconds
    data: bytes  # full link-layer frame


def read_pcap(path: str) -> list[Packet]:
    """Parse a classic pcap file into packets, in capture order.

    Raises :class:`ValueError` for a file that is smaller than the global header or has an unknown magic.
    A pcapng capture has an unknown magic.
    The parser drops a truncated last record. Thus a capture that was killed during a write is still valid.
    """
    with open(path, "rb") as fh:
        blob = fh.read()
    if len(blob) < 24:
        raise ValueError(f"{path}: too small to be a pcap")
    (magic,) = struct.unpack("<I", blob[:4])
    if magic in (PCAP_MAGIC_LE, PCAP_MAGIC_NS_LE):
        endian, nanos = "<", magic == PCAP_MAGIC_NS_LE
    elif magic in (PCAP_MAGIC_BE, PCAP_MAGIC_NS_BE):
        endian, nanos = ">", magic == PCAP_MAGIC_NS_BE
    else:
        raise ValueError(
            f"{path}: unsupported pcap magic 0x{magic:08x} (pcapng is not supported; capture with classic pcap)"
        )
    packets: list[Packet] = []
    off = 24  # global header
    rec = endian + "IIII"
    # Integer nanoseconds, not float seconds. At an epoch of 1.75e9 s, the float ULP is approximately 238 ns.
    # That removes the sub-microsecond differences that the analyzers report.
    frac_to_ns = 1 if nanos else 1000
    while off + 16 <= len(blob):
        ts_sec, ts_frac, caplen, _origlen = struct.unpack(rec, blob[off : off + 16])
        off += 16
        if off + caplen > len(blob):
            break
        packets.append(Packet(ts=ts_sec * 1_000_000_000 + ts_frac * frac_to_ns, data=blob[off : off + caplen]))
        off += caplen
    return packets


@dataclass
class L4Info:
    """The transport-layer view of one captured frame."""

    proto: int
    payload: bytes
    src_ip: str
    dst_ip: str
    src_port: int
    dst_port: int
    seq: int  # TCP sequence number; 0 for UDP
    flags: int  # TCP flag byte; 0 for UDP

    @property
    def flow(self) -> Flow:
        return (self.src_ip, self.dst_ip, self.src_port, self.dst_port)


def l4_info(frame: bytes) -> L4Info | None:
    """Extract the transport layer of an Ethernet/IPv4 UDP or TCP frame. Return None for all other frames.

    A double-tagged 802.1Q frame returns None, because the parser skips only one tag.
    An IP fragment that is not the first returns None, because it has no L4 header.
    A header length below the minimum also returns None.
    """
    if len(frame) < 14:
        return None
    ethertype = struct.unpack(">H", frame[12:14])[0]
    l3 = frame[14:]
    if ethertype == ETHERTYPE_VLAN and len(frame) >= 18:
        ethertype = struct.unpack(">H", frame[16:18])[0]
        l3 = frame[18:]
    if ethertype != ETHERTYPE_IPV4 or len(l3) < 20:
        return None
    ihl = (l3[0] & 0x0F) * 4
    if ihl < 20:
        return None
    proto = l3[9]
    total_len = struct.unpack(">H", l3[2:4])[0]
    frag_off = struct.unpack(">H", l3[6:8])[0]
    if (frag_off & 0x1FFF) != 0:
        return None
    src = socket.inet_ntoa(l3[12:16])
    dst = socket.inet_ntoa(l3[16:20])
    # A frame that is shorter than the Ethernet minimum has padding on the wire.
    # The IP total length gives the end of the datagram. Thus the padding is not in the payload.
    l3 = l3[:total_len] if 0 < total_len <= len(l3) else l3
    l4 = l3[ihl:]
    if proto == IPPROTO_UDP:
        if len(l4) < 8:
            return None
        sport, dport = struct.unpack(">HH", l4[0:4])
        return L4Info(proto, l4[8:], src, dst, sport, dport, seq=0, flags=0)
    if proto == IPPROTO_TCP:
        if len(l4) < 20:
            return None
        sport, dport, seq = struct.unpack(">HHI", l4[0:8])
        data_off = (l4[12] >> 4) * 4
        if data_off < 20:
            return None
        return L4Info(proto, l4[data_off:], src, dst, sport, dport, seq=seq, flags=l4[13])
    return None


def reassemble_tcp_flow(
    segs: list[tuple[int, int, bytes]], base: int | None = None
) -> tuple[bytes, list[tuple[int, int]], tuple[int, ...]]:
    """Join the ``(seq, stamp, payload)`` segments of one TCP connection direction into its byte stream.

    Returns the stream, a sorted ``(offset, stamp)`` list with the start of each segment, and the offsets
    of the bytes after a capture gap. *stamp* is not changed.
    *base* is the sequence number of the first data byte, ISN + 1.
    Without it, the first segment in *segs* is the lowest. Thus a reordered capture loses all bytes before it.
    The function drops a full retransmit and trims a partial retransmit.
    A segment that is not in the capture gives no bytes. The stream continues with the next captured byte.
    The gap offset tells a parser that the bytes on the two sides are not contiguous.
    """
    base = segs[0][0] if base is None else base
    ordered = sorted(((seq - base) & 0xFFFFFFFF, ts, payload) for seq, ts, payload in segs)
    buf = bytearray()
    starts: list[tuple[int, int]] = []
    gaps: list[int] = []
    next_rel = 0  # sequence offset of the next byte of the stream, if there is no gap
    for rel, ts, payload in ordered:
        end = rel + len(payload)
        if rel < next_rel:
            skip = next_rel - rel
            if skip >= len(payload):
                continue
            payload = payload[skip:]
        elif rel > next_rel:
            gaps.append(len(buf))
        starts.append((len(buf), ts))
        buf.extend(payload)
        next_rel = end
    return bytes(buf), starts, tuple(gaps)


def ts_at_offset(starts: list[tuple[int, int]]) -> TsAt:
    """Make a ``ts_at(offset)`` callback that returns the timestamp of the segment that has byte *offset*."""
    offsets = [o for o, _ in starts]

    def ts_at(pos: int) -> int:
        i = bisect.bisect_right(offsets, pos) - 1
        return starts[i][1] if i >= 0 else starts[0][1]

    return ts_at


@dataclass
class Stream:
    """One transport byte stream: a UDP datagram, or one direction of one TCP connection."""

    proto: int
    flow: Flow
    data: bytes
    ts_at: TsAt  # capture timestamp of the byte at a given offset into ``data``
    # Capture index of the packet that has that byte. This is the capture order, which a clock step cannot change.
    order_at: TsAt
    # Offsets into ``data`` of the bytes after a capture gap. A message across a gap is corrupt.
    gaps: tuple[int, ...] = ()


def streams(packets: list[Packet]) -> list[Stream]:
    """Split captured packets into byte streams: UDP datagrams in capture order, then TCP connection directions.

    A SYN on a 4-tuple starts a new connection.
    Thus a peer that connects again from the same port gives a second stream.
    The function omits a TCP direction without payload, for example only a handshake. It keeps all UDP datagrams.
    """
    out: list[Stream] = []
    # One entry for each connection direction, in the order of the first packet. The dict maps a 4-tuple to its
    # current entry. If the capture has the SYN, the exact base is known. Thus a reordered capture cannot truncate it.
    connections: list[tuple[Flow, int | None, list]] = []
    live: dict[Flow, list] = {}
    for index, pkt in enumerate(packets):
        info = l4_info(pkt.data)
        if info is None:
            continue
        if info.proto == IPPROTO_UDP:
            ts = pkt.ts
            out.append(Stream(IPPROTO_UDP, info.flow, info.payload, lambda _pos, ts=ts: ts, lambda _pos, i=index: i))
            continue
        segs = live.get(info.flow)
        if segs is None or (info.flags & TCP_SYN):
            segs = []
            live[info.flow] = segs
            base = (info.seq + 1) & 0xFFFFFFFF if info.flags & TCP_SYN else None
            connections.append((info.flow, base, segs))
        if info.payload:
            segs.append((info.seq, (pkt.ts, index), info.payload))
    for flow, base, segs in connections:
        if not segs:
            continue
        data, starts, gaps = reassemble_tcp_flow(segs, base)
        if starts:
            ts_at = ts_at_offset([(o, stamp[0]) for o, stamp in starts])
            order_at = ts_at_offset([(o, stamp[1]) for o, stamp in starts])
            out.append(Stream(IPPROTO_TCP, flow, data, ts_at, order_at, gaps))
    return out


def _eth_ipv4(proto: int, l4: bytes, src: str, dst: str) -> bytes:
    ip = struct.pack(">BBHHHBBH", 0x45, 0, 20 + len(l4), 0, 0, 64, proto, 0)
    eth = b"\x02\x00\x00\x00\x00\x02" + b"\x02\x00\x00\x00\x00\x01" + struct.pack(">H", ETHERTYPE_IPV4)
    return eth + ip + socket.inet_aton(src) + socket.inet_aton(dst) + l4


def build_udp_frame(
    payload: bytes, *, src: str = "10.0.0.1", dst: str = "10.0.0.2", sport: int = 34501, dport: int = 34491
) -> bytes:
    """Build an Ethernet/IPv4/UDP frame around *payload*, for analyzer self-tests."""
    return _eth_ipv4(IPPROTO_UDP, struct.pack(">HHHH", sport, dport, 8 + len(payload), 0) + payload, src, dst)


def build_tcp_frame(
    payload: bytes,
    seq: int,
    *,
    src: str = "10.0.0.1",
    dst: str = "10.0.0.2",
    sport: int = 34501,
    dport: int = 34491,
    flags: int = TCP_ACK,
) -> bytes:
    """Build an Ethernet/IPv4/TCP frame around *payload*, for analyzer self-tests.

    The TCP header has the minimum 20 bytes. Only the sequence number and the flags are important for reassembly.
    """
    tcp = struct.pack(">HHIIBBHHH", sport, dport, seq & 0xFFFFFFFF, 0, 5 << 4, flags, 0, 0, 0)
    return _eth_ipv4(IPPROTO_TCP, tcp + payload, src, dst)


def write_pcap(path: str | Path, records: list[tuple[int, bytes]], *, nanos: bool = True) -> None:
    """Write ``(ts_ns, frame)`` records as a little-endian classic pcap, for analyzer self-tests.

    If *nanos* is false, the file uses the microsecond magic and truncates each timestamp to microseconds.
    """
    magic = PCAP_MAGIC_NS_LE if nanos else PCAP_MAGIC_LE
    with open(path, "wb") as fh:
        fh.write(struct.pack("<IHHiIII", magic, 2, 4, 0, 0, 65535, 1))
        for ts_ns, data in records:
            sec, rem = divmod(ts_ns, 1_000_000_000)
            frac = rem if nanos else rem // 1000
            fh.write(struct.pack("<IIII", sec, frac, len(data), len(data)))
            fh.write(data)


def selftest() -> int:
    """Check the parser and the stream builder with synthetic captures with known contents. 0 is pass."""
    tmp = Path(tempfile.mkdtemp(prefix="pcap_reader_"))
    ok = True

    def check(cond: bool, msg: str) -> None:
        nonlocal ok
        print(("  PASS " if cond else "  FAIL ") + msg)
        ok = ok and cond

    base = 1_750_000_000_123_456_789

    # The two magics write and read back correctly. The microsecond magic truncates to whole microseconds.
    frame = build_udp_frame(b"hello")
    for nanos, want in ((True, base), (False, base - base % 1000)):
        path = tmp / f"magic_{nanos}.pcap"
        write_pcap(path, [(base, frame)], nanos=nanos)
        pkts = read_pcap(str(path))
        check(len(pkts) == 1 and pkts[0].ts == want, f"nanos={nanos}: timestamp {pkts[0].ts} == {want}")
        check(pkts[0].data == frame, f"nanos={nanos}: frame bytes round-trip")

    # A big-endian writer stores the magic in its own byte order. A little-endian read sees it swapped.
    be = tmp / "be.pcap"
    with open(be, "wb") as fh:
        fh.write(struct.pack(">IHHiIII", PCAP_MAGIC_NS_LE, 2, 4, 0, 0, 65535, 1))
        fh.write(struct.pack(">IIII", base // 1_000_000_000, base % 1_000_000_000, len(frame), len(frame)) + frame)
    check(read_pcap(str(be))[0].ts == base, "big-endian nanosecond pcap parses")

    # A pcapng section header and a file shorter than the global header raise an error.
    for name, blob in (("pcapng", b"\x0a\x0d\x0d\x0a" + bytes(40)), ("short", b"\xd4\xc3\xb2\xa1")):
        path = tmp / f"{name}.pcap"
        path.write_bytes(blob)
        try:
            read_pcap(str(path))
            check(False, f"{name}: ValueError raised")
        except ValueError:
            check(True, f"{name}: ValueError raised")

    # The parser drops a record truncated by a killed capture. It keeps the complete record before it.
    trunc = tmp / "trunc.pcap"
    write_pcap(trunc, [(base, frame), (base + 1, frame)])
    trunc.write_bytes(trunc.read_bytes()[:-3])
    check(len(read_pcap(str(trunc))) == 1, "truncated final record dropped")

    # The parser skips one 802.1Q tag. An IP fragment that is not the first has no L4 header.
    tagged = frame[:12] + struct.pack(">HH", ETHERTYPE_VLAN, 7) + frame[12:]
    info = l4_info(tagged)
    check(info is not None and info.payload == b"hello", "VLAN-tagged frame parses")
    frag = bytearray(frame)
    frag[14 + 6 : 14 + 8] = struct.pack(">H", 0x0010)
    check(l4_info(bytes(frag)) is None, "non-first IP fragment ignored")

    # A UDP datagram is one stream with one timestamp for every byte.
    udp = streams([Packet(base, build_udp_frame(b"abcdef", sport=1, dport=2))])
    check(len(udp) == 1 and udp[0].data == b"abcdef", "UDP datagram becomes one stream")
    check(udp[0].flow == ("10.0.0.1", "10.0.0.2", 1, 2), f"UDP flow carries both ports (got {udp[0].flow})")
    check(udp[0].ts_at(0) == udp[0].ts_at(5) == base, "UDP stream has a constant timestamp")

    # TCP across the 32-bit wrap without a captured SYN. The parser drops a full retransmit and trims a partial overlap.
    isn = 0xFFFFFFF8
    segs = [
        (base + 0, build_tcp_frame(b"ABCDEFGH", isn)),
        (base + 10, build_tcp_frame(b"ABCDEFGH", isn)),  # full retransmit
        (base + 20, build_tcp_frame(b"IJKL", isn + 8)),
        (base + 30, build_tcp_frame(b"KLMN", isn + 10)),  # overlaps "KL"
    ]
    tcp = streams([Packet(ts, f) for ts, f in segs])
    check(len(tcp) == 1 and tcp[0].data == b"ABCDEFGHIJKLMN", f"TCP reassembled across wrap (got {tcp[0].data!r})")
    check(tcp[0].ts_at(0) == base and tcp[0].ts_at(7) == base, "bytes 0-7 carry the first segment's timestamp")
    check(tcp[0].ts_at(8) == base + 20, "bytes 8-11 carry the third segment's timestamp")
    check(tcp[0].ts_at(13) == base + 30, "the trimmed overlap's bytes carry its own timestamp")

    # With the captured SYN, a segment that arrives before a lower segment goes to its correct position.
    reordered = [
        Packet(base, build_tcp_frame(b"", isn - 1, flags=TCP_SYN)),
        Packet(base + 20, build_tcp_frame(b"IJKL", isn + 8)),
        Packet(base + 30, build_tcp_frame(b"ABCDEFGH", isn)),
    ]
    ro = streams(reordered)
    check(
        len(ro) == 1 and ro[0].data == b"ABCDEFGHIJKL", f"SYN-based reassembly survives reordering (got {ro[0].data!r})"
    )
    check(ro[0].ts_at(0) == base + 30 and ro[0].ts_at(8) == base + 20, "reordered bytes keep their own timestamps")

    # A segment that is not in the capture gives a gap marker between the bytes on its two sides.
    gap = streams([Packet(base, build_tcp_frame(b"AAAA", 100)), Packet(base + 1, build_tcp_frame(b"CCCC", 108))])
    check(gap[0].data == b"AAAACCCC" and gap[0].gaps == (4,), "gap in sequence space is marked, not truncated")
    check(gap[0].ts_at(4) == base + 1, "bytes after a gap keep their own timestamp")

    # Two connections on one 4-tuple, with a SYN between them, are two streams, also with unrelated ISNs.
    reconnect = [
        Packet(base, build_tcp_frame(b"", 1000, flags=TCP_SYN)),
        Packet(base + 1, build_tcp_frame(b"first", 1001)),
        Packet(base + 2, build_tcp_frame(b"", 50, flags=TCP_SYN)),
        Packet(base + 3, build_tcp_frame(b"second", 51)),
    ]
    rc = streams(reconnect)
    check([s.data for s in rc] == [b"first", b"second"], f"SYN splits a reused 4-tuple (got {[s.data for s in rc]})")

    # The two directions of one connection are separate streams with mirrored flows.
    duplex = streams(
        [
            Packet(base, build_tcp_frame(b"req", 1, sport=50000, dport=13400)),
            Packet(base + 5, build_tcp_frame(b"rsp", 9, src="10.0.0.2", dst="10.0.0.1", sport=13400, dport=50000)),
        ]
    )
    flows = sorted(s.flow for s in duplex)
    check(
        flows == [("10.0.0.1", "10.0.0.2", 50000, 13400), ("10.0.0.2", "10.0.0.1", 13400, 50000)],
        f"each direction is its own stream (got {flows})",
    )

    # A clock step back puts the timestamps out of order, but the capture index follows the stream.
    stepped = streams([Packet(base, build_tcp_frame(b"AB", 1)), Packet(base - 10**9, build_tcp_frame(b"CD", 3))])
    check(stepped[0].ts_at(2) < stepped[0].ts_at(0), "a clock step shows in ts_at")
    check((stepped[0].order_at(0), stepped[0].order_at(3)) == (0, 1), "order_at gives each byte's capture index")

    # A pure ACK with padding to the Ethernet minimum of 60 bytes.
    # The IP total length keeps the padding out of the payload.
    ack = build_tcp_frame(b"", 7, flags=TCP_ACK)
    padded = ack + bytes(60 - len(ack))
    info = l4_info(padded)
    check(info is not None and info.payload == b"", f"padding is not payload (got {info and info.payload!r})")
    check(streams([Packet(base, padded)]) == [], "a padded pure ACK yields no stream")

    # Linux sends 12 bytes of TCP timestamp options, data offset 8. The payload starts after them.
    opts = bytearray(build_tcp_frame(bytes(12) + b"DATA", 1))
    opts[14 + 20 + 12] = 8 << 4
    info = l4_info(bytes(opts))
    check(info is not None and info.payload == b"DATA", f"TCP options skipped (got {info and info.payload!r})")

    # A header length below the protocol minimum is incorrect. It is not a shorter header.
    bad_ihl = bytearray(frame)
    bad_ihl[14] = 0x44
    check(l4_info(bytes(bad_ihl)) is None, "IHL below 5 rejected")
    bad_off = bytearray(build_tcp_frame(b"x", 1))
    bad_off[14 + 20 + 12] = 4 << 4
    check(l4_info(bytes(bad_off)) is None, "TCP data offset below 5 rejected")

    # Only a handshake has no bytes and gives no stream.
    check(streams([Packet(base, build_tcp_frame(b"", 1, flags=TCP_SYN | TCP_ACK))]) == [], "handshake-only omitted")

    print("SELFTEST", "PASSED" if ok else "FAILED")
    return 0 if ok else 1


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--selftest", action="store_true", help="run the built-in synthetic self-test and exit")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    ap.error("pcap_reader is a library; the only command is --selftest")


if __name__ == "__main__":
    sys.exit(main())
