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

"""Receives the SOME/IP-SD traffic of the ETS service on the multicast group, for the SD timing tests."""

from __future__ import annotations

import asyncio
import contextlib
import socket
import struct
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager

from scapy.contrib.automotive.someip import (
    SD,
    SDENTRY_TYPE_SRV_FINDSERVICE,
    SDENTRY_TYPE_SRV_OFFERSERVICE,
    SOMEIP,
    SDEntry_Service,
)

from someip_itf.ets.constants import (
    ETS_INSTANCE_ID,
    ETS_REQUIRED_INSTANCE_ID,
    ETS_SERVICE_ID,
    SD_MULTICAST_GROUP,
    SD_PORT,
)


def open_sd_multicast_socket(tester_ip: str) -> tuple[socket.socket, bytes]:
    """Return a non-blocking socket on ``SD_PORT`` that is a member of the SD group on *tester_ip*, and its ``mreq``.

    The socket uses the wildcard address. A socket bound to *tester_ip* does not receive group traffic.
    The caller must drop the membership with the returned ``mreq`` and close the socket.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", SD_PORT))
    mreq = struct.pack(
        "4s4s",
        socket.inet_aton(SD_MULTICAST_GROUP),
        socket.inet_aton(tester_ip),
    )
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(tester_ip))
    sock.setblocking(False)
    return sock, mreq


def close_sd_multicast_socket(sock: socket.socket, mreq: bytes) -> None:
    """Drop the group membership from ``open_sd_multicast_socket``, and close *sock*."""
    with contextlib.suppress(OSError):
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_DROP_MEMBERSHIP, mreq)
    sock.close()


@asynccontextmanager
async def sd_multicast_socket(tester_ip: str) -> AsyncIterator[tuple[socket.socket, bytes]]:
    """Yield the socket and ``mreq`` of ``open_sd_multicast_socket``. On exit, leave the group and close the socket."""
    sock, mreq = open_sd_multicast_socket(tester_ip)
    try:
        yield sock, mreq
    finally:
        close_sd_multicast_socket(sock, mreq)


def is_ets_sd_entry(data: bytes, entry_type: int, inst_id: int | None = None) -> bool:
    """Return True if ``data`` has a SOME/IP-SD entry with the given type for the ETS service."""
    if len(data) < 16:
        return False
    try:
        pkt = SOMEIP(data)
        if pkt.srv_id != 0xFFFF or pkt.sub_id != 0x8100:
            return False
        raw_payload = b"".join(d.load for d in pkt.data if hasattr(d, "load"))
        if not raw_payload:
            return False
        sd_layer = SD(raw_payload)
        for entry in sd_layer.entry_array or []:
            if (
                isinstance(entry, SDEntry_Service)
                and entry.type == entry_type
                and entry.srv_id == ETS_SERVICE_ID
                and (inst_id is None or entry.inst_id == inst_id)
            ):
                return True
    except Exception:  # noqa: BLE001 — ignore a malformed SD message
        pass
    return False


def is_ets_find(data: bytes) -> bool:
    """Return True if ``data`` is a SOME/IP-SD FindService for the instance that the DUT ETS client mode finds."""
    return is_ets_sd_entry(data, SDENTRY_TYPE_SRV_FINDSERVICE, ETS_REQUIRED_INSTANCE_ID)


def is_ets_offer(data: bytes) -> bool:
    """Return True if ``data`` is a SOME/IP-SD OfferService for the ETS service/instance."""
    return is_ets_sd_entry(data, SDENTRY_TYPE_SRV_OFFERSERVICE, ETS_INSTANCE_ID)


async def ets_offer_seen_within(tester_ip: str, timeout: float) -> bool:
    """Join the SD group on a new socket. Return True if an ETS OfferService arrives in *timeout* seconds."""
    loop = asyncio.get_running_loop()
    async with sd_multicast_socket(tester_ip) as (sock, _mreq):
        deadline = loop.time() + timeout
        while (remaining := deadline - loop.time()) > 0:
            try:
                data = await asyncio.wait_for(loop.sock_recv(sock, 65535), timeout=remaining)
            except asyncio.TimeoutError:
                return False
            if is_ets_offer(data):
                return True
    return False
