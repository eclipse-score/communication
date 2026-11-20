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

"""Tests for the pairing of sends to egress frames by the counter that each sender writes into its payload."""

from __future__ import annotations

import struct

import pytest
from pcap_reader import build_tcp_frame, build_udp_frame, read_pcap, write_pcap
from wire_latency import (
    COUNTER_FIELDS,
    HEAD_LEN,
    AppSend,
    Frame,
    _build_someip,
    expected_bytes,
    load_app_log,
    pair_app_log,
    reassemble_frames,
    selftest,
)

SVC = 0x0103
PORT = 60010
GAP_NS = 1_000_000  # the 1 ms gap between sends
LATENCY_NS = 50_000
T0 = 1_000_000_000


def _frame(event_id: int, payload: bytes, ts: int) -> Frame:
    return Frame(
        service_id=SVC,
        event_id=event_id,
        session_id=0,
        first_ts=ts,
        last_ts=ts,
        payload_len=len(payload),
        dst_ip="10.0.0.2",
        dst_port=PORT,
        head=payload[:HEAD_LEN],
    )


def _bytes_payload(counter: int, size: int = 16) -> bytes:
    """Return the wire payload of the Bytes event: a 4-byte length field, the big-endian counter, then padding."""
    return struct.pack(">I", size - 4) + struct.pack(">I", counter) + bytes(size - 8)


def _sends(event_id: int, size: int, counters) -> list[AppSend]:
    return [AppSend(event_id, size, ctr, T0 + ctr * GAP_NS) for ctr in counters]


def _run(event_id: int, sends: list[AppSend], frames: list[Frame], type_name: str | None = None):
    fields = {event_id: COUNTER_FIELDS[type_name]} if type_name else {}
    return pair_app_log(frames, sends, [PORT], counter_fields=fields)


def _latencies(paired) -> list[int]:
    return [p.latency_ns for p in paired]


def test_mid_bucket_drop_pairs_the_later_frames_with_their_own_sends():
    event_id = 0x66
    sends = _sends(event_id, 16, range(1, 6))
    frames = [_frame(event_id, _bytes_payload(s.counter), s.t_app_ns + LATENCY_NS) for s in sends if s.counter != 3]

    paired, dropped, by_port = _run(event_id, sends, frames, "Bytes")

    assert _latencies(paired) == [LATENCY_NS] * 4
    assert dropped == 1
    assert by_port == {PORT: 1}


@pytest.mark.parametrize("order", [">", "<"])
def test_multibyte_tag_matches_in_either_byte_order(order: str):
    event_id = 0x78
    sends = _sends(event_id, 2, range(300, 305))
    frames = [_frame(event_id, struct.pack(order + "H", s.counter), s.t_app_ns + LATENCY_NS) for s in sends]

    paired, dropped, _ = _run(event_id, sends, frames, "Uint16")

    assert _latencies(paired) == [LATENCY_NS] * 5
    assert dropped == 0


def test_bool_detects_a_single_drop_by_parity():
    event_id = 0x81
    sends = _sends(event_id, 1, range(0, 5))
    frames = [
        _frame(event_id, struct.pack("?", bool(s.counter & 1)), s.t_app_ns + LATENCY_NS)
        for s in sends
        if s.counter != 2
    ]

    paired, dropped, _ = _run(event_id, sends, frames, "Bool")

    assert _latencies(paired) == [LATENCY_NS] * 4
    assert dropped == 1


def test_float_tag_is_the_ieee754_encoding_of_the_counter():
    event_id = 0x7F
    sends = _sends(event_id, 4, (7000, 7001, 7002))
    frames = [
        _frame(event_id, struct.pack(">f", float(s.counter)), s.t_app_ns + LATENCY_NS)
        for s in sends
        if s.counter != 7001
    ]

    paired, dropped, _ = _run(event_id, sends, frames, "Float")

    assert _latencies(paired) == [LATENCY_NS, LATENCY_NS]
    assert dropped == 1


def test_nested_struct_tag_sits_after_the_leading_members():
    event_id = 0x84
    sends = _sends(event_id, 1024, (10, 11, 12))
    frames = [
        _frame(event_id, bytes(55) + struct.pack(">I", s.counter) + bytes(1024 - 59), s.t_app_ns + LATENCY_NS)
        for s in sends
        if s.counter != 11
    ]

    paired, dropped, _ = _run(event_id, sends, frames, "NestedStruct")

    assert _latencies(paired) == [LATENCY_NS, LATENCY_NS]
    assert dropped == 1


def test_stray_frame_before_the_first_send_is_not_paired():
    event_id = 0x66
    sends = _sends(event_id, 16, (1, 2))
    stray = _frame(event_id, _bytes_payload(1), sends[0].t_app_ns - 10 * GAP_NS)
    frames = [stray] + [_frame(event_id, _bytes_payload(s.counter), s.t_app_ns + LATENCY_NS) for s in sends]

    paired, dropped, _ = _run(event_id, sends, frames, "Bytes")

    assert _latencies(paired) == [LATENCY_NS, LATENCY_NS]
    assert dropped == 0


def test_frame_without_a_matching_send_is_skipped_without_moving_the_cursor():
    event_id = 0x66
    sends = _sends(event_id, 16, (1, 2))
    unlogged = _frame(event_id, _bytes_payload(900), sends[0].t_app_ns + LATENCY_NS)
    frames = [unlogged] + [_frame(event_id, _bytes_payload(s.counter), s.t_app_ns + LATENCY_NS) for s in sends]

    paired, dropped, _ = _run(event_id, sends, frames, "Bytes")

    assert _latencies(paired) == [LATENCY_NS, LATENCY_NS]
    assert dropped == 0


def test_unknown_type_pairs_by_order_and_time_only():
    event_id = 0x90
    sends = _sends(event_id, 8, (1, 2, 3))
    frames = [_frame(event_id, bytes(8), s.t_app_ns + LATENCY_NS) for s in sends]

    paired, dropped, _ = _run(event_id, sends, frames)

    assert _latencies(paired) == [LATENCY_NS] * 3
    assert dropped == 0


def test_unknown_type_takes_the_latest_send_not_after_the_frame():
    event_id = 0x90
    sends = _sends(event_id, 8, range(1, 6))
    frames = [_frame(event_id, bytes(8), s.t_app_ns + LATENCY_NS) for s in sends if s.counter != 3]

    paired, dropped, _ = _run(event_id, sends, frames)

    assert _latencies(paired) == [LATENCY_NS] * 4
    assert dropped == 1


def test_send_without_a_counter_matches_any_frame():
    event_id = 0x66
    sends = [AppSend(event_id, 16, None, T0 + i * GAP_NS) for i in (1, 2)]
    frames = [_frame(event_id, _bytes_payload(5), s.t_app_ns + LATENCY_NS) for s in sends]

    paired, dropped, _ = _run(event_id, sends, frames, "Bytes")

    assert _latencies(paired) == [LATENCY_NS, LATENCY_NS]
    assert dropped == 0


def test_payload_too_short_for_the_field_matches_any_frame():
    event_id = 0x66
    sends = _sends(event_id, 6, (1, 2))
    frames = [_frame(event_id, struct.pack(">I", 2) + bytes(2), s.t_app_ns + LATENCY_NS) for s in sends]

    paired, dropped, _ = _run(event_id, sends, frames, "Bytes")

    assert _latencies(paired) == [LATENCY_NS, LATENCY_NS]
    assert dropped == 0


def test_sends_after_the_last_frame_are_dropped():
    event_id = 0x66
    sends = _sends(event_id, 16, (1, 2, 3))
    frames = [_frame(event_id, _bytes_payload(1), sends[0].t_app_ns + LATENCY_NS)]

    paired, dropped, _ = _run(event_id, sends, frames, "Bytes")

    assert _latencies(paired) == [LATENCY_NS]
    assert dropped == 2


def test_expected_bytes_covers_both_byte_orders_and_the_sender_casts():
    assert expected_bytes("B", 0x1FF) == {b"\xff"}
    assert expected_bytes("H", 0x0102) == {b"\x01\x02", b"\x02\x01"}
    assert expected_bytes("?", 3) == {b"\x01"}
    assert expected_bytes("f", 2) == {struct.pack(">f", 2.0), struct.pack("<f", 2.0)}


def test_tp_head_is_assembled_from_the_segments_that_overlap_it(tmp_path):
    """A NestedStruct counter at offset 55 is after a 48-byte first segment, in the second segment."""
    event_id = 0x84
    payload = bytes(55) + struct.pack(">I", 42) + bytes(1024 - 59)
    seg0 = _build_someip(SVC, event_id, 0x10, payload[:48], tp_offset=0, tp_more=True)
    seg1 = _build_someip(SVC, event_id, 0x10, payload[48:], tp_offset=48, tp_more=False)
    pcap = tmp_path / "tp.pcap"
    write_pcap(pcap, [(T0, build_udp_frame(seg0)), (T0 + 1000, build_udp_frame(seg1))])

    frames = reassemble_frames(read_pcap(str(pcap)))

    assert len(frames) == 1
    assert frames[0].payload_len == 1024
    assert frames[0].head[55:59] == struct.pack(">I", 42)


def test_tcp_capture_gap_loses_only_the_message_it_cuts(tmp_path):
    """A TCP segment that is not in the capture loses only its message, not the remaining connection."""
    event_id = 0x66
    messages = [_build_someip(SVC, event_id, ctr, _bytes_payload(ctr, 300)) for ctr in (1, 2, 3, 4)]
    stream = b"".join(messages)
    isn = 5000
    # 100-byte segments. The segment at offset 400 is in message 2 (bytes 316 to 632) and is not in the capture.
    captured = [(off, stream[off : off + 100]) for off in range(0, len(stream), 100) if off != 400]
    pcap = tmp_path / "gap.pcap"
    write_pcap(pcap, [(T0 + k * 1000, build_tcp_frame(seg, isn + off)) for k, (off, seg) in enumerate(captured)])
    sends = [AppSend(event_id, 300, ctr, T0 - (10 - ctr) * GAP_NS) for ctr in (1, 2, 3, 4)]

    frames = reassemble_frames(read_pcap(str(pcap)))
    paired, dropped, _ = pair_app_log(frames, sends, None, {event_id: COUNTER_FIELDS["Bytes"]})

    assert [f.head[4:8] for f in frames] == [struct.pack(">I", c) for c in (1, 3, 4)]
    assert len(paired) == 3
    assert dropped == 1


def test_load_app_log_reads_the_counter_and_skips_bad_rows(tmp_path):
    log = tmp_path / "sendlog.csv"
    log.write_text("event_id,size,counter,t_app_ns\n0x66,16,7,1000\n0x66,16,,2000\n0x66,16,seven,3000\n0x66,16\n")

    sends = load_app_log(str(log))

    assert [(s.counter, s.t_app_ns) for s in sends] == [(7, 1000), (None, 2000)]


def test_load_app_log_without_a_counter_column_loads_none(tmp_path):
    log = tmp_path / "sendlog.csv"
    log.write_text("event_id,size,t_app_ns\n0x66,16,1000\n")

    sends = load_app_log(str(log))

    assert [(s.counter, s.t_app_ns) for s in sends] == [(None, 1000)]


def test_selftest_passes():
    assert selftest() == 0
