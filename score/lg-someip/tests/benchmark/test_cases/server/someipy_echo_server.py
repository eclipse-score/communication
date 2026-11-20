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

"""someipy Echo server for the SOME/IP RPC benchmark.

It offers the RPC service to the SD group. It answers each Echo request with the payload of the request.
The server time has no effect on the metric. The client-side capture puts all of the server time into
the wire round trip, which the analyzer subtracts.
``OFFERING`` on stdout is the ready marker for the orchestrator.
"""

from __future__ import annotations

import argparse
import asyncio
import logging
import signal
import sys

from someipy import (
    Method,
    MethodResult,
    ServerServiceInstance,
    ServiceBuilder,
    TransportLayerProtocol,
)
from someipy.someipy_logging import set_someipy_log_level
from someip_bench.someipyd import start_daemon, stop_daemon

# The time to live of the SD offer, in seconds. The value 0 makes each offer a StopOffer.
_OFFER_TTL_S = 5
_CYCLIC_OFFER_DELAY_MS = 1000


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="someipy Echo server")
    p.add_argument("--server-ip", required=True, help="Host address for the Echo endpoint and someipyd.")
    p.add_argument("--server-port", type=int, default=30509)
    p.add_argument("--sd-address", default="224.224.224.245")
    p.add_argument("--sd-port", type=int, default=30490)
    p.add_argument("--service-id", type=lambda s: int(s, 0), default=0x0104)
    p.add_argument("--instance-id", type=lambda s: int(s, 0), default=0x0001)
    p.add_argument("--method-id", type=lambda s: int(s, 0), default=0x0001)
    p.add_argument("--major-version", type=int, default=1)
    p.add_argument("--minor-version", type=int, default=0)
    p.add_argument("--socket-path", default="/tmp/someipyd.sock")
    return p.parse_args()


def _echo(payload: bytes, _source) -> MethodResult:
    result = MethodResult()
    result.payload = payload
    return result


async def _run(args: argparse.Namespace) -> int:
    """Offer the service until a signal comes. Return 1 if the someipyd socket did not appear, else 0."""
    set_someipy_log_level(logging.WARNING)
    started = await start_daemon(
        sd_address=args.sd_address,
        sd_port=args.sd_port,
        interface=args.server_ip,
        socket_path=args.socket_path,
        log_level="WARNING",
    )
    if started is None:
        print("SERVER ERROR: someipyd socket never appeared", flush=True)
        return 1

    daemon, daemon_proc = started
    service = (
        ServiceBuilder()
        .with_service_id(args.service_id)
        .with_major_version(args.major_version)
        .with_minor_version(args.minor_version)
        .with_method(Method(id=args.method_id, protocol=TransportLayerProtocol.UDP, method_handler=_echo))
        .build()
    )
    server = ServerServiceInstance(
        daemon=daemon,
        service=service,
        instance_id=args.instance_id,
        endpoint_ip=args.server_ip,
        endpoint_port=args.server_port,
        ttl=_OFFER_TTL_S,
        cyclic_offer_delay_ms=_CYCLIC_OFFER_DELAY_MS,
    )
    await server.start_offer()
    print("OFFERING", flush=True)

    loop = asyncio.get_running_loop()
    stop = asyncio.Event()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, stop.set)
    await stop.wait()

    await server.stop_offer()
    await stop_daemon(daemon, daemon_proc)
    return 0


def main() -> int:
    try:
        return asyncio.run(_run(_parse_args()))
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
