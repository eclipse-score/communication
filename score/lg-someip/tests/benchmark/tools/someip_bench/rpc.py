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

"""One SOME/IP RPC benchmark run.

The daemon, the Echo client and tcpdump run on the target. The someipy Echo server runs on the host.
The capture runs on the board, next to the client. Thus the call log and the wire timestamps use one clock.
"""

from __future__ import annotations

import time
from dataclasses import dataclass
from pathlib import Path
from typing import Final

from perf_itf.artifacts import outputs_dir, pull_file
from perf_itf.sampling import stop_capture

from someip_bench.run import RPC_CLIENT_APP, SomeipBoardRun, Topology
from someip_bench.subscriber import OFFERING_TIMEOUT_S, start_echo_server

# The client discovery timeout is 30 s. Its 4050 calls take approximately 15 s on a board.
# Thus this value only stops a hang.
_CLIENT_DONE_TIMEOUT_S: Final = 180

# Longer than the BPF read timeout of tcpdump. Thus the capture writes all frames before it stops.
_CAPTURE_DRAIN_S: Final = 2


@dataclass(frozen=True)
class RpcResult:
    """The artifacts of one run, on the host: the client call log and the client-side pcap."""

    pcap_path: str
    log_path: str


class SomeipRpcBenchmark(SomeipBoardRun):
    """The RPC benchmark on one board.

    The Echo server offers before the client starts. Thus the first find of the client gets a cyclic offer.
    someipd silently drops a find that is older than its SD time to live.
    Then the client has a timeout, although someipd operates correctly.
    """

    def __init__(self, target, topology: Topology, *, tmp_path: Path, server: Path) -> None:
        """*server* is the host-side Echo server script."""
        super().__init__(target, topology, app=RPC_CLIENT_APP, transport="udp", tmp_path=tmp_path)
        self._server = server

    def run(self) -> RpcResult:
        """Run the capture, someipd, the host server and the client. Copy the call log and pcap to the host."""
        target, paths = self._target, self._paths
        server_log = str(outputs_dir(self._tmp_path) / "echo_server.log")
        with self._board_session(server_log) as session:
            session.host = start_echo_server(
                self._server,
                server_ip=self._topology.subscriber_ip,
                log_path=server_log,
                socket_path=session.socket_path,
            )
            session.host.wait_for_marker("OFFERING", OFFERING_TIMEOUT_S)
            client = self._launch_app(f"SOMEIP_BENCH_SENDLOG={paths.sendlog}")
            session.procs.append(client)
            self._await_app(client, _CLIENT_DONE_TIMEOUT_S, "rpc client")
            # tcpdump empties its BPF buffer only when the buffer is full or at its read timeout.
            # At this call rate, neither occurs before SIGTERM. SIGTERM discards the buffer, which has the last frames.
            time.sleep(_CAPTURE_DRAIN_S)

            session.host.stop()
            session.host = None
            stop_capture(target, qnx=True, log=paths.capture_log)
            session.stopped = True

            result = RpcResult(str(self._tmp_path / "client.pcap"), str(self._tmp_path / "calls.csv"))
            assert pull_file(target, paths.pcap, result.pcap_path), "failed to pull the client pcap"
            assert pull_file(target, paths.sendlog, result.log_path), "failed to pull the call log"
            return result
