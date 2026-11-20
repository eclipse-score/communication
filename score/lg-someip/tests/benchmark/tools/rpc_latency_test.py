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

"""Tests for the RPC client processing time analyzer, with synthetic requests and responses."""

from __future__ import annotations

import csv
import dataclasses
import struct

import pytest
from pcap_reader import build_udp_frame, write_pcap
from rpc_latency import (
    MSGTYPE_REQUEST,
    MSGTYPE_RESPONSE,
    RPC_METHOD_ID,
    RPC_SERVICE_ID,
    RpcCall,
    analyze_rpc,
    analyze_rpc_files,
    render_rpc_table,
    write_rpc_csv,
)
from wire_latency import SomeIpSegment

CLIENT = 0x0001
T0 = 1_000_000_000
GAP_NS = 1_000_000
WIRE_NS = 200_000  # request egress to response ingress
BEFORE_NS = 30_000  # t_req_ns to request egress
AFTER_NS = 20_000  # response ingress to t_resp_ns


def _payload(counter: int, size: int) -> bytes:
    """Return the wire payload of an Echo request: a 4-byte length field, the big-endian counter, then padding."""
    return struct.pack(">I", size - 4) + struct.pack(">I", counter) + bytes(size - 8)


def _someip(msg_type: int, session: int, payload: bytes, service: int = RPC_SERVICE_ID, method: int = RPC_METHOD_ID):
    header = struct.pack(
        ">IIIBBBB", (service << 16) | method, 8 + len(payload), (CLIENT << 16) | session, 1, 1, msg_type, 0
    )
    return header + payload


def _segment(msg_type: int, session: int, payload: bytes, ts: int, service=RPC_SERVICE_ID, method=RPC_METHOD_ID):
    return SomeIpSegment(
        ts=ts,
        end_ts=ts,
        service_id=service,
        method_id=method,
        length=8 + len(payload),
        client_id=CLIENT,
        session_id=session,
        proto_version=1,
        iface_version=1,
        msg_type=msg_type,
        return_code=0,
        is_tp=False,
        tp_offset=0,
        tp_more=False,
        payload=payload,
    )


def _exchange(counter: int, size: int, session: int, wire_ns: int = WIRE_NS):
    """Return one answered call: its log row and its request and response segments."""
    t_req_wire = T0 + counter * GAP_NS
    t_resp_wire = t_req_wire + wire_ns
    call = RpcCall(size, counter, t_req_wire - BEFORE_NS, t_resp_wire + AFTER_NS)
    request = _segment(MSGTYPE_REQUEST, session, _payload(counter, size), t_req_wire)
    response = _segment(MSGTYPE_RESPONSE, session, _payload(counter, size), t_resp_wire)
    return call, [request, response]


def _run(exchanges):
    calls = [call for call, _ in exchanges]
    segments = [seg for _, segs in exchanges for seg in segs]
    return analyze_rpc(calls, segments)


def _stat(analysis, variant: str, phase: str):
    return next(s for s in analysis.stats if s.variant == variant and s.phase == phase)


def test_client_is_app_rtt_minus_wire_rtt():
    analysis = _run([_exchange(ctr, 64, ctr) for ctr in (1, 2, 3)])

    assert analysis.dropped == 0
    assert [s.phase for s in analysis.stats] == ["client", "app_rtt", "wire_rtt"]
    for phase, micros in (("client", 50.0), ("app_rtt", 250.0), ("wire_rtt", 200.0)):
        stat = _stat(analysis, "64B", phase)
        assert stat.count == 2  # the first iteration is the cold-start outlier
        assert (stat.minimum, stat.maximum) == (micros, micros)


def test_wire_rtt_runs_from_the_request_end_to_the_response_start():
    """For a request in many captured packets, the wire RTT starts at its last byte."""
    call, (request, response) = _exchange(1, 64, 1)
    request = dataclasses.replace(request, ts=request.ts - 5_000)

    analysis = _run([_exchange(0, 64, 9), (call, [request, response])])

    assert _stat(analysis, "64B", "wire_rtt").minimum == 200.0
    assert _stat(analysis, "64B", "client").minimum == 50.0


def test_stats_are_ordered_by_numeric_size_then_phase():
    sizes = (1396, 64, 8, 512)
    exchanges = [_exchange(10 * k + i, size, 10 * k + i) for k, size in enumerate(sizes) for i in range(3)]

    analysis = _run(exchanges)

    assert [(s.variant, s.phase) for s in analysis.stats] == [
        (f"{size}B", phase) for size in (8, 64, 512, 1396) for phase in ("client", "app_rtt", "wire_rtt")
    ]


def test_responses_out_of_order_pair_by_session():
    exchanges = [_exchange(ctr, 64, ctr) for ctr in (1, 2, 3)]
    calls = [call for call, _ in exchanges]
    requests = [segs[0] for _, segs in exchanges]
    # All responses arrive after all requests, the last response first.
    responses = [
        _segment(MSGTYPE_RESPONSE, segs[1].session_id, segs[1].payload, T0 + 10 * GAP_NS - segs[1].session_id)
        for _, segs in exchanges
    ]

    # The newest segment is first in the list. The pairing uses the capture timestamps, not the list order.
    analysis = analyze_rpc(calls, list(reversed(requests + responses)))

    # Counter 1 is the first iteration and is not in the statistics. Counters 2 and 3 keep the egress of their request.
    wire = sorted((T0 + 10 * GAP_NS - ctr) - (T0 + ctr * GAP_NS) for ctr in (2, 3))
    stat = _stat(analysis, "64B", "wire_rtt")
    assert analysis.dropped == 0
    assert (stat.minimum, stat.maximum) == (wire[0] / 1e3, wire[1] / 1e3)


def test_other_service_and_method_are_ignored():
    exchanges = [_exchange(ctr, 64, ctr) for ctr in (1, 2, 3)]
    # Without the filter, a foreign response with the same request ID that arrives first gets the pairing.
    foreign = [
        _segment(MSGTYPE_RESPONSE, 2, _payload(2, 64), T0 + 2 * GAP_NS + 1_000, service=0x0105),
        _segment(MSGTYPE_RESPONSE, 3, _payload(3, 64), T0 + 3 * GAP_NS + 1_000, method=0x0002),
        _segment(MSGTYPE_REQUEST, 3, b"\x00", T0 + 3 * GAP_NS + 2_000, service=0x0105),
    ]
    calls = [call for call, _ in exchanges]
    segments = [seg for _, segs in exchanges for seg in segs] + foreign

    analysis = analyze_rpc(calls, segments)

    assert analysis.dropped == 0
    stat = _stat(analysis, "64B", "wire_rtt")
    assert (stat.minimum, stat.maximum) == (200.0, 200.0)


def test_wrapped_session_pairs_with_the_latest_request():
    # The first call with session 5 gets no answer. The session ID wraps and is used again for counter 3.
    unanswered = _segment(MSGTYPE_REQUEST, 5, _payload(1, 64), T0 + GAP_NS)
    exchanges = [_exchange(2, 64, 2), _exchange(3, 64, 5), _exchange(4, 64, 6)]
    calls = [RpcCall(64, 1, T0 + GAP_NS - BEFORE_NS, 0)] + [call for call, _ in exchanges]  # counter 1 timed out
    segments = [unanswered] + [seg for _, segs in exchanges for seg in segs]

    analysis = analyze_rpc(calls, segments)

    assert analysis.dropped == 1
    stat = _stat(analysis, "64B", "wire_rtt")
    assert stat.count == 2  # counters 3 and 4; counter 2 is the first iteration
    assert (stat.minimum, stat.maximum) == (200.0, 200.0)


def test_lost_response_and_failed_call_are_dropped():
    exchanges = [_exchange(ctr, 64, ctr) for ctr in (1, 2, 3, 4, 5)]
    lost = exchanges[2][1][:1]  # counter 3: request captured, response not
    failed = exchanges[3][0]  # counter 4: the client logged no response
    exchanges[2] = (exchanges[2][0], lost)
    exchanges[3] = (RpcCall(failed.size, failed.counter, failed.t_req_ns, 0), exchanges[3][1])

    analysis = _run(exchanges)

    assert analysis.dropped == 2
    assert _stat(analysis, "64B", "client").count == 2  # counters 2 and 5


def test_missing_request_is_dropped():
    exchanges = [_exchange(ctr, 64, ctr) for ctr in (1, 2, 3)]
    exchanges[1] = (exchanges[1][0], exchanges[1][1][1:])

    analysis = _run(exchanges)

    assert analysis.dropped == 1


@pytest.mark.parametrize(
    "payload",
    [
        struct.pack(">I", 61) + struct.pack(">I", 1) + bytes(56),  # length is not equal to the 60 data bytes
        struct.pack(">I", 1) + bytes(60),  # counter without the vector length field
        struct.pack(">I", 3) + b"\x00\x00\x01",  # too short for a counter
    ],
)
def test_request_without_a_matching_length_field_raises(payload: bytes):
    call = RpcCall(64, 1, T0 - BEFORE_NS, T0 + WIRE_NS + AFTER_NS)
    segments = [
        _segment(MSGTYPE_REQUEST, 1, payload, T0),
        _segment(MSGTYPE_RESPONSE, 1, payload, T0 + WIRE_NS),
    ]

    with pytest.raises(ValueError):
        analyze_rpc([call], segments)


def test_files_end_to_end(tmp_path):
    exchanges = [_exchange(ctr, size, ctr) for ctr, size in ((1, 8), (2, 8), (3, 8), (4, 1396), (5, 1396), (6, 1396))]
    log = tmp_path / "rpc_calls.csv"
    with open(log, "w", newline="") as handle:
        handle.write("size,counter,t_req_ns,t_resp_ns\n")
        for call, _ in exchanges:
            handle.write(f"{call.size},{call.counter},{call.t_req_ns},{call.t_resp_ns}\n")
        handle.write(f"8,7,{T0 + 7 * GAP_NS},0\n")
    pcap = tmp_path / "client.pcap"
    write_pcap(
        pcap,
        [
            (seg.ts, build_udp_frame(_someip(seg.msg_type, seg.session_id, seg.payload)))
            for _, segs in exchanges
            for seg in segs
        ],
    )

    analysis = analyze_rpc_files(log, pcap)

    assert analysis.dropped == 1
    assert [s.variant for s in analysis.stats] == ["8B"] * 3 + ["1396B"] * 3
    for variant in ("8B", "1396B"):
        assert _stat(analysis, variant, "client").minimum == 50.0
        assert _stat(analysis, variant, "wire_rtt").maximum == 200.0


def test_table_reports_the_dropped_calls():
    exchanges = [_exchange(ctr, 64, ctr) for ctr in (1, 2, 3)]
    calls = [call for call, _ in exchanges] + [RpcCall(64, 4, T0, 0)]
    analysis = analyze_rpc(calls, [seg for _, segs in exchanges for seg in segs])

    table = render_rpc_table(analysis)

    assert table.endswith("\n\ndropped calls: 1")
    assert "client processing (app - wire)" in table


def test_csv_has_one_row_per_stat(tmp_path):
    analysis = _run([_exchange(10 * k + i, size, 10 * k + i) for k, size in enumerate((8, 64)) for i in range(3)])
    path = tmp_path / "rpc_stats.csv"

    write_rpc_csv(analysis, path)

    with open(path, newline="") as handle:
        rows = list(csv.DictReader(handle))
    assert [(r["variant"], r["phase"]) for r in rows] == [(s.variant, s.phase) for s in analysis.stats]
    assert len(rows) == 6
    assert float(rows[0]["mean"]) == 50.0
