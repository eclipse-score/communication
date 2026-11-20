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

"""Test for the SOME/IP RPC client processing time.

The ``rpc_run`` fixture runs the Echo client one time against the someipy server on the host.
It gives the call log and the client-side pcap.
The test makes a table per payload size from them and writes a CSV to the test outputs.

The test makes sure that each size has samples and that a maximum of one tenth of the calls is lost.
It also makes sure that no client time is zero or negative.
In one clock domain, such a value can only be a pairing fault.
The test does not compare latency to a limit.
"""

from __future__ import annotations

import logging

from perf_itf.artifacts import outputs_dir
from rpc_latency import analyze_rpc_files, load_call_log, render_rpc_table, write_rpc_csv

# pytest hides stdout when a test passes. The ITF pytest configuration shows this logger.
logger = logging.getLogger("someip_benchmark")

_SIZES = ("8B", "64B", "512B", "1396B")


def test_rpc_client_processing_time(rpc_run, tmp_path):
    """Log the RPC table. Make sure that each size has pairs, few calls are lost and no client time is <= 0."""
    analysis = analyze_rpc_files(rpc_run.log_path, rpc_run.pcap_path)
    table = render_rpc_table(analysis)
    logger.info("SOME/IP RPC client processing time (UDP):\n%s", table)
    # Write the CSV before the asserts, so that the outputs of a failed run also have the analyzer data.
    write_rpc_csv(analysis, outputs_dir(tmp_path) / "rpc_latency.csv")

    paired = {s.variant for s in analysis.stats if s.phase == "client"}
    missing = [size for size in _SIZES if size not in paired]
    assert not missing, f"no paired calls for {missing} -- see the client log and the capture"
    calls = len(load_call_log(rpc_run.log_path))
    assert analysis.dropped * 10 <= calls, f"{analysis.dropped} of {calls} calls dropped -- see the client log"
    negative = [s.variant for s in analysis.stats if s.phase == "client" and s.minimum <= 0]
    assert not negative, f"client processing time <= 0 for {negative}: request and response frames are mispaired"
