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

"""someipy subscriber for the SOME/IP benchmark.

someipy is an independent SOME/IP stack. Thus a subscription from it makes someipd serialize
and send real notifications on the wire. The latency analyzer measures this wire traffic.
A consumer on the same host uses shared memory and sends nothing on the wire.
``SUBSCRIBED <n>`` on stdout is the ready marker for the orchestrator.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import signal
import sys
from pathlib import Path

from someipy import (
    ClientServiceInstance,
    Event,
    EventGroup,
    ServiceBuilder,
    TransportLayerProtocol,
)
from someipy.someipy_logging import set_someipy_log_level
from someip_bench.someipyd import start_daemon, stop_daemon

# Base event IDs of the 15 benchmark events: the wire method ID without the 0x8000 notification bit.
# They must agree with the kEvt* constants of the sender. The EventGroup below uses them again as method IDs.
EVENT_BASE_IDS = (
    0x65,
    0x66,
    0x78,
    0x79,
    0x7A,
    0x7B,
    0x7C,
    0x7D,
    0x7E,
    0x7F,
    0x80,
    0x81,
    0x82,
    0x83,
    0x84,
)


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="someipy benchmark subscriber")
    p.add_argument("--num-clients", type=int, default=1)
    p.add_argument(
        "--client-ip",
        required=True,
        help="Subscriber endpoint IP (different from the someipd unicast address).",
    )
    p.add_argument(
        "--client-base-port",
        type=int,
        default=60010,
        help="Client i binds client-base-port + i. The orchestrator always gives this value. The analyzer labels "
        "samples by these ports, thus a manual run must use the same base at both ends.",
    )
    p.add_argument(
        "--sd-address",
        default="224.224.224.245",
        help="SD multicast group. someipy joins only 224.x groups. The SD group of the deploy must be this value.",
    )
    p.add_argument("--sd-port", type=int, default=30490)
    p.add_argument(
        "--unicast-sd",
        action="store_true",
        help="Accept a unicast OfferService, for a target that sends its offer directly to this host.",
    )
    p.add_argument("--service-id", type=lambda s: int(s, 0), default=0x0103)
    p.add_argument("--instance-id", type=lambda s: int(s, 0), default=0x0091)
    p.add_argument("--major-version", type=int, default=2)
    p.add_argument("--minor-version", type=int, default=1)
    p.add_argument("--eventgroup-id", type=int, default=5)
    p.add_argument("--transport", choices=("udp", "tcp"), default="udp")
    p.add_argument("--discovery-timeout", type=float, default=15.0)
    p.add_argument("--socket-path", default="/tmp/someipyd.sock")
    p.add_argument(
        "--summary-path",
        default="",
        help="File for the JSON summary of received counts, written at exit.",
    )
    return p.parse_args()


async def _run(args: argparse.Namespace) -> int:
    """Subscribe, count notifications until a signal comes, and write the summary. Return the exit code.

    The exit code is the only machine-readable diagnosis for the orchestrator, which shows it as it is:
    0 after a correct run with a summary, 1 if the someipyd socket did not appear, 2 if not all clients
    found the service in ``--discovery-timeout``.
    A return value that is not zero writes no summary file.
    """
    set_someipy_log_level(logging.INFO)
    protocol = TransportLayerProtocol.TCP if args.transport == "tcp" else TransportLayerProtocol.UDP

    # The unicast SD script is the someipy daemon with the unicast-offer patch.
    # The daemon has 10 s to bind its socket, before the discovery timeout starts.
    # The marker timeout of the orchestrator must include the two times.
    started = await start_daemon(
        sd_address=args.sd_address,
        sd_port=args.sd_port,
        interface=args.client_ip,
        socket_path=args.socket_path,
        log_level="INFO",
        script=Path(__file__).resolve().parent / "someipyd_unicast_sd.py" if args.unicast_sd else None,
    )
    if started is None:
        print("SUBSCRIBER ERROR: someipyd socket never appeared", flush=True)
        return 1

    daemon, daemon_proc = started

    eventgroup = EventGroup(
        id=args.eventgroup_id,
        events=[Event(id=0x8000 | base, protocol=protocol) for base in EVENT_BASE_IDS],
    )
    service = (
        ServiceBuilder()
        .with_service_id(args.service_id)
        .with_major_version(args.major_version)
        .with_minor_version(args.minor_version)
        .with_eventgroup(eventgroup)
        .build()
    )
    clients = [
        ClientServiceInstance(
            daemon=daemon,
            service=service,
            instance_id=args.instance_id,
            endpoint_ip=args.client_ip,
            endpoint_port=args.client_base_port + i,
        )
        for i in range(args.num_clients)
    ]

    loop = asyncio.get_running_loop()
    deadline = loop.time() + args.discovery_timeout
    while loop.time() < deadline:
        avail = await asyncio.gather(*[c.is_available() for c in clients])
        if all(avail):
            break
        await asyncio.sleep(0.5)
    else:
        print(
            f"SUBSCRIBER ERROR: not all clients discovered service within {args.discovery_timeout}s",
            flush=True,
        )
        await stop_daemon(daemon, daemon_proc)
        return 2

    received = [0] * args.num_clients

    def _make_cb(idx: int):
        def _cb(_method_id: int, _payload: bytes) -> None:
            received[idx] += 1

        return _cb

    for i, client in enumerate(clients):
        client.register_callback(_make_cb(i))
        client.subscribe_eventgroup(eventgroup, 30)

    # The ready marker. For the orchestrator, the subscription is in place at this line.
    print(f"SUBSCRIBED {args.num_clients}", flush=True)

    # someipy subscribes again at each cyclic offer while the loop is free.
    # Thus it automatically tries again a first subscribe that came before the offer and got a NACK.
    stop = asyncio.Event()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, stop.set)
    await stop.wait()

    for client in clients:
        client.unsubscribe_eventgroup(eventgroup)
        client.register_callback(None)
    await stop_daemon(daemon, daemon_proc)

    summary = {
        "num_clients": args.num_clients,
        "received": received,
        "total": sum(received),
    }
    line = "SUBSCRIBER SUMMARY " + json.dumps(summary)
    print(line, flush=True)
    if args.summary_path:
        with open(args.summary_path, "w") as f:
            json.dump(summary, f)
    return 0


def main() -> int:
    args = _parse_args()
    try:
        return asyncio.run(_run(args))
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
