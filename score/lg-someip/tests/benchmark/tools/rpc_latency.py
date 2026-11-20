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

"""Analyzer for the SOME/IP RPC client processing time: ``client = app RTT - wire RTT``.

The client stamps all times.
The app RTT comes from the client CLOCK_REALTIME stamps around one blocking Echo call.
The wire RTT is the time from request egress to response ingress, in a capture on the client NIC.
The difference is the time that the client uses between its API and the wire, in the two directions.
The server processing and the link are in the wire RTT and cancel.
A request pairs with its response by the SOME/IP request ID.
It pairs with its log row by the call counter. The client writes this counter big-endian into the first four
data bytes, directly after the 4-byte length field of the ``vector<uint8>``.
"""

from __future__ import annotations

import csv
import dataclasses
import struct
from dataclasses import dataclass
from pathlib import Path

from pcap_reader import read_pcap
from perf_itf.benchmark.stats import PhaseStat, analyze, render_table
from wire_latency import SomeIpSegment, extract_segments

RPC_SERVICE_ID = 0x0104
RPC_METHOD_ID = 0x0001
MSGTYPE_REQUEST = 0x00
MSGTYPE_RESPONSE = 0x80

# Display order of the rows for one size. Only "client" is a KPI. The two RTTs explain it.
PHASES = ("client", "app_rtt", "wire_rtt")
PHASE_LABELS = {
    "client": "client processing (app - wire)",
    "app_rtt": "app round trip",
    "wire_rtt": "wire round trip",
}


@dataclass(frozen=True)
class RpcCall:
    """One logged Echo call. ``t_resp_ns`` is 0 if the call failed or had a timeout."""

    size: int
    counter: int
    t_req_ns: int
    t_resp_ns: int


@dataclass(frozen=True)
class RpcAnalysis:
    """Statistics per size in display order, and the number of logged calls without a sample."""

    stats: list[PhaseStat]
    dropped: int


def load_call_log(path: str | Path) -> list[RpcCall]:
    """Read the ``size,counter,t_req_ns,t_resp_ns`` CSV of the client."""
    with open(path, newline="") as handle:
        return [
            RpcCall(int(row["size"]), int(row["counter"]), int(row["t_req_ns"]), int(row["t_resp_ns"]))
            for row in csv.DictReader(handle)
        ]


def _request_counter(segment: SomeIpSegment) -> int:
    """Return the call counter of an Echo request, after a check that the payload starts with the length field.

    Raises ``ValueError`` if not. Without this check, a stack with a different vector serialization pairs nothing.
    """
    payload = segment.payload
    if len(payload) < 8 or struct.unpack(">I", payload[:4])[0] != len(payload) - 4:
        raise ValueError(
            f"Echo request (session {segment.session_id}) does not start with a 4-byte length field "
            f"matching its {len(payload)}-byte payload"
        )
    return struct.unpack(">I", payload[4:8])[0]


def pair_wire(segments: list[SomeIpSegment]) -> dict[int, tuple[SomeIpSegment, SomeIpSegment]]:
    """Map the call counter of each answered request to its ``(request, response)`` segments.

    A response matches the latest unanswered request with the same client and session ID.
    Thus a session ID that wraps during a long run pairs with the correct call.
    """
    pending: dict[tuple[int, int], SomeIpSegment] = {}
    paired: dict[int, tuple[SomeIpSegment, SomeIpSegment]] = {}
    for seg in sorted(segments, key=lambda s: s.ts):
        if seg.service_id != RPC_SERVICE_ID or seg.method_id != RPC_METHOD_ID:
            continue
        key = (seg.client_id, seg.session_id)
        if seg.msg_type == MSGTYPE_REQUEST:
            pending[key] = seg
        elif seg.msg_type == MSGTYPE_RESPONSE and key in pending:
            request = pending.pop(key)
            paired[_request_counter(request)] = (request, seg)
    return paired


def analyze_rpc(calls: list[RpcCall], segments: list[SomeIpSegment]) -> RpcAnalysis:
    """Calculate client, app RTT and wire RTT statistics per size from the logged calls and the captured segments.

    A call is dropped if it failed in the client, or if its request or response is not in the capture.
    """
    wire = pair_wire(segments)
    samples: list[dict] = []
    dropped = 0
    for call in calls:
        frames = wire.get(call.counter)
        if not call.t_resp_ns or frames is None:
            dropped += 1
            continue
        request, response = frames
        app_ns = call.t_resp_ns - call.t_req_ns
        wire_ns = response.ts - request.end_ts
        for phase, duration_ns in (("client", app_ns - wire_ns), ("app_rtt", app_ns), ("wire_rtt", wire_ns)):
            samples.append(
                {"variant": f"{call.size}B", "phase": phase, "iteration": call.counter, "duration_ns": duration_ns}
            )
    # analyze sorts unknown variants alphabetically. That puts 1396B before 8B.
    stats = sorted(analyze(samples), key=lambda s: (int(s.variant[:-1]), PHASES.index(s.phase)))
    return RpcAnalysis(stats, dropped)


def analyze_rpc_files(log_path: str | Path, pcap_path: str | Path) -> RpcAnalysis:
    """Apply :func:`analyze_rpc` to the client call log and the client-side pcap."""
    return analyze_rpc(load_call_log(log_path), extract_segments(read_pcap(pcap_path)))


def render_rpc_table(analysis: RpcAnalysis) -> str:
    """Return the statistics as a fixed-width table in microseconds, with the dropped count below it."""
    return f"{render_table(analysis.stats, PHASE_LABELS)}\n\ndropped calls: {analysis.dropped}"


def write_rpc_csv(analysis: RpcAnalysis, path: str | Path) -> None:
    """Write one row for each (size, phase) statistic."""
    fields = [f.name for f in dataclasses.fields(PhaseStat)]
    with open(path, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(dataclasses.asdict(s) for s in analysis.stats)
