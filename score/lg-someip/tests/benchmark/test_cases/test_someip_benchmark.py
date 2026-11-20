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

"""Tests for the SOME/IP send-path benchmark: latency in one run, CPU and memory usage in a different run.

The ``benchmark_run`` fixture runs one benchmark and gives its artifacts.
A latency run (``--measure=latency``) gives the pcap, the send-log and the subscriber summary.
``test_send_path_latency`` calculates the ``T_wire - T_app`` latency per event from them.
A resource run (``--measure=resources``) gives the subscriber summary and the CPU and memory data per process.
``test_resource_usage`` shows these data.
The sampler makes the target slower. Thus it never runs while the test measures latency.
The latency CSV, the logs and the pcap go to the test outputs.

The assertions only make sure that the full pipeline ran.
The tests do not compare latency, CPU usage, memory usage or drop count to a limit.
Thus a passed run is not a passed measurement. Read the tables.
"""

from __future__ import annotations

import logging

import pytest
from perf_itf.artifacts import outputs_dir
from perf_itf.sampling import render_cpu_table, render_mem_table
from wire_latency import analyze_app_log, render_table, write_csv

# The benchmark output is its result tables. pytest hides stdout when a test passes.
# Thus write to a logger, which the ITF pytest configuration shows.
logger = logging.getLogger("someip_benchmark")

# Base event ID to type label.
# Each ID must agree with the kEvt* constants of the sender and with the event map. If not, the analyzer pairs nothing.
# The transport label comes from the run. Thus one table is applicable to UDP and TCP.
_EVENT_TYPES = {
    0x65: "Uint8",
    0x66: "Bytes",
    0x78: "Uint16",
    0x79: "Uint32",
    0x7A: "Uint64",
    0x7B: "Int8",
    0x7C: "Int16",
    0x7D: "Int32",
    0x7E: "Int64",
    0x7F: "Float",
    0x80: "Double",
    0x81: "Bool",
    0x82: "Struct",
    0x83: "FixedArray",
    0x84: "NestedStruct",
}


def _assert_subscribed(result) -> None:
    """Make sure that each subscriber received notifications, thus the stack had the load of the run."""
    summary = result.subscriber_summary
    assert summary, "subscriber produced no summary (it never subscribed or was killed early)"
    assert summary.get("total", 0) > 0, "subscriber received no notifications"
    for i in range(result.num_clients):
        assert summary["received"][i] > 0, f"subscriber c{i} received no notifications"


def test_send_path_latency(benchmark_run, tmp_path):
    """Log the latency table. Only make sure that the pipeline made data."""
    result = benchmark_run
    transport_label = result.transport.upper()
    event_map = {base: {"type": label, "transport": transport_label} for base, label in _EVENT_TYPES.items()}

    stats = analyze_app_log(result.sendlog_path, result.pcap_path, event_map, result.client_ports)
    latency_table = render_table(stats)
    logger.info(
        "SOME/IP send-path latency (%s, %d subscriber(s)):\n%s",
        transport_label,
        result.num_clients,
        latency_table,
    )
    # Write the CSV before the asserts, so that the outputs of a failed run also have the analyzer data.
    write_csv(stats, str(outputs_dir(tmp_path) / "send_path_latency.csv"))

    _assert_subscribed(result)
    assert stats, (
        "no send-path latency samples were paired -- the subscription must complete "
        "before emission and the capture must be running (see the streamed logs)"
    )


@pytest.mark.resources
def test_resource_usage(benchmark_run):
    """Log the CPU and memory tables. Only make sure that the sampler made data."""
    result = benchmark_run
    logger.info(
        "CPU usage during run (peak is representative; the mean is diluted by the grace-period idle):\n%s",
        render_cpu_table(result.cpu_stats),
    )
    logger.info(
        "Memory usage during run (RSS includes mapped shared memory; private excludes it):\n%s",
        render_mem_table(result.mem_stats),
    )

    _assert_subscribed(result)
    assert result.cpu_stats, "CPU sampling produced no data"
    assert result.mem_stats, "memory sampling produced no data"
