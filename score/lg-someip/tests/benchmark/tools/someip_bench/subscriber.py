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

"""The someipy peers that the benchmarks run on the host (subscriber and Echo server), and their service identities."""

from __future__ import annotations

import contextlib
import json
import logging
import os
import signal
import subprocess
import sys
from pathlib import Path
from typing import Final

from perf_itf.waiting import wait_until
from rpc_latency import RPC_METHOD_ID, RPC_SERVICE_ID

_logger = logging.getLogger("someip_benchmark")

SD_GROUP: Final = "224.224.224.245"
"""The SD group for the offers. someipy joins only 224.x multicast groups."""
SD_PORT: Final = 30490

# Service identity. It must agree with the benchmark service model and the event map.
SERVICE_ID: Final = 0x0103
INSTANCE_ID: Final = 0x0091
MAJOR: Final = 2
MINOR: Final = 1
EVENTGROUP: Final = 5
CLIENT_BASE_PORT: Final = 60010

# The remaining RPC service identity that the host Echo server offers.
# The service and method IDs come from the analyzer, which pairs frames by them.
# The board clients use the same identity in their configuration and service model.
RPC_INSTANCE_ID: Final = 0x0001
RPC_SERVER_PORT: Final = 30509

# The outer wait must be longer than the inner wait by the subscriber startup time.
# Thus a subscriber that cannot find the service reports this itself, and the run does not report only a marker timeout.
# The script waits up to 10 s for its someipyd socket before the discovery timeout starts.
DISCOVERY_TIMEOUT_S: Final = 30
SUBSCRIBER_STARTUP_BUDGET_S: Final = 15
SUBSCRIBED_TIMEOUT_S: Final = DISCOVERY_TIMEOUT_S + SUBSCRIBER_STARTUP_BUDGET_S

# The Echo server does not do discovery. It only must start: the someipyd socket wait and the offer.
OFFERING_TIMEOUT_S: Final = SUBSCRIBER_STARTUP_BUDGET_S


def client_ports(num_clients: int) -> dict[int, str]:
    """Map the endpoint port of each subscriber to its label. The analyzer uses these labels for its rows.

    The subscriber binds :data:`CLIENT_BASE_PORT` and the ports above it.
    Only this function sets which port is for which subscriber.
    """
    return {CLIENT_BASE_PORT + i: f"c{i}" for i in range(num_clients)}


class HostProcess:
    """A host-side SOME/IP peer script (someipy subscriber or Echo server) that connects to someipd on the target.

    It runs in its own process group. Thus one signal goes to the full tree.
    The script starts ``someipy.someipyd`` as a different process.
    That child, not the script, binds the host SD port and joins the multicast group.
    """

    def __init__(self, process: subprocess.Popen, log_path: str) -> None:
        self._process = process
        self._log_path = log_path

    @classmethod
    def spawn(cls, argv: list, log_path: str) -> HostProcess:
        """Start *argv* with this interpreter. Write its output to *log_path*."""
        env = {**os.environ, "PYTHONPATH": os.pathsep.join(sys.path)}
        # The child duplicates the fd. Thus the parent can close its handle when Popen returns.
        with open(log_path, "wb") as log:
            process = subprocess.Popen(
                [sys.executable, *argv],
                stdout=log,
                stderr=subprocess.STDOUT,
                env=env,
                start_new_session=True,
            )
        return cls(process, log_path)

    def read_log(self) -> str:
        """Return the full log, or an empty string if the log does not exist."""
        try:
            return Path(self._log_path).read_text(errors="replace")
        except OSError:
            return ""

    def wait_for_marker(self, marker: str, timeout: float) -> None:
        """Wait until the script prints *marker*. Fail immediately if the script exits without the marker.

        The script prints an error line before it exits. Its startup or discovery timeout is shorter than *timeout*.
        If this function only monitors the log, it waits too long and then reports the incorrect process.
        """

        def printed() -> bool:
            if marker in self.read_log():
                return True
            if self._process.poll() is not None:
                # Read again. The marker can arrive between the two checks above.
                text = self.read_log()
                if marker in text:
                    return True
                raise RuntimeError(
                    f"host process exited with code {self._process.returncode} before printing '{marker}'\n"
                    f"{text[-2000:]}"
                )
            return False

        if not wait_until(printed, timeout=timeout, interval=0.3):
            raise RuntimeError(f"host process never printed '{marker}' within {timeout}s\n{self.read_log()[-2000:]}")

    def stop(self, grace_s: float = 10) -> None:
        """Stop the script and its someipyd child. It is safe to call this more than one time.

        SIGTERM goes only to the script. Then the script stops its daemon, and the subscriber writes its summary.
        Then SIGKILL goes to the group, also if the script stopped.
        A someipyd that continues to run holds the host SD port and the client ports.
        Then the bind of the next run fails, and that failure shows as a discovery timeout.
        """
        if self._process.poll() is None:
            self._process.terminate()
            try:
                self._process.wait(timeout=grace_s)
            except subprocess.TimeoutExpired:
                _logger.warning("host process did not exit within %ss; killing its process group", grace_s)
        # start_new_session sets the script pid as the group ID. getpgid fails after the script is reaped.
        with contextlib.suppress(OSError):
            os.killpg(self._process.pid, signal.SIGKILL)
        self._process.wait()


def start_subscriber(
    script: Path,
    *,
    subscriber_ip: str,
    transport: str,
    num_clients: int,
    summary_path: str,
    log_path: str,
    socket_path: str,
    unicast_sd: bool,
) -> HostProcess:
    """Start the someipy subscriber *script* with its log at *log_path* and its summary at *summary_path*."""
    argv = [
        script,
        "--num-clients",
        str(num_clients),
        "--client-ip",
        subscriber_ip,
        "--transport",
        transport,
        "--sd-address",
        SD_GROUP,
        "--sd-port",
        str(SD_PORT),
        "--service-id",
        hex(SERVICE_ID),
        "--instance-id",
        hex(INSTANCE_ID),
        "--major-version",
        str(MAJOR),
        "--minor-version",
        str(MINOR),
        "--eventgroup-id",
        str(EVENTGROUP),
        "--client-base-port",
        str(CLIENT_BASE_PORT),
        "--discovery-timeout",
        str(DISCOVERY_TIMEOUT_S),
        "--summary-path",
        summary_path,
        "--socket-path",
        socket_path,
    ]
    return HostProcess.spawn(argv + (["--unicast-sd"] if unicast_sd else []), log_path)


def start_echo_server(script: Path, *, server_ip: str, log_path: str, socket_path: str) -> HostProcess:
    """Start the someipy Echo server *script*. It offers the RPC service on *server_ip* to the SD group."""
    argv = [
        script,
        "--server-ip",
        server_ip,
        "--server-port",
        str(RPC_SERVER_PORT),
        "--sd-address",
        SD_GROUP,
        "--sd-port",
        str(SD_PORT),
        "--service-id",
        hex(RPC_SERVICE_ID),
        "--instance-id",
        hex(RPC_INSTANCE_ID),
        "--method-id",
        hex(RPC_METHOD_ID),
        "--socket-path",
        socket_path,
    ]
    return HostProcess.spawn(argv, log_path)


def read_summary(summary_path: str) -> dict:
    """Read the JSON summary of the subscriber. Return an empty dict if there is no summary or it is incomplete.

    The test asserts on an empty dict. Thus a truncated file must not cause a JSONDecodeError.
    """
    try:
        with open(summary_path) as fh:
            return json.load(fh)
    except (OSError, json.JSONDecodeError):
        return {}
