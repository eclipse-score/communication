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

"""Sends to the ETS service over a plain UDP socket. Tests use it for frames that ScapySomeipClient cannot build."""

from __future__ import annotations

import asyncio
import socket

import pytest
from scapy.contrib.automotive.someip import SD, SDENTRY_TYPE_EVTGRP_SUBSCRIBE_ACK, SOMEIP, SDEntry_EventGroup

from someip_itf.codec import L4Proto, SdEntry
from someip_itf.ets.constants import (
    ETS_MAJOR_VERSION,
    ETS_SERVER_UDP_PORT,
    EVENTGROUP_UNICAST,
    METHOD_CALL_TIMEOUT,
    SD_PORT,
)
from someip_itf.frames import someip_frame

NO_RESPONSE_TIMEOUT = 3.0
"""Time that a test waits for no answer. After this time, the test accepts that the DUT dropped the frame."""


def ets_frame(
    service_id: int, method_id: int, payload: bytes, *, iface_ver: int = ETS_MAJOR_VERSION, **header
) -> bytes:
    """Build a :func:`someip_itf.frames.someip_frame`. The default interface version is the ETS version."""
    return someip_frame(service_id, method_id, payload, iface_ver=iface_ver, **header)


def open_bound_socket(port: int) -> socket.socket:
    """Return a non-blocking UDP socket bound to *port* on all interfaces."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", port))
    sock.setblocking(False)
    return sock


async def expect_no_response(loop, sock, timeout: float = NO_RESPONSE_TIMEOUT) -> None:
    """Fail the test if *sock* receives data in *timeout* seconds."""
    try:
        data, _ = await asyncio.wait_for(loop.sock_recvfrom(sock, 65535), timeout=timeout)
    except asyncio.TimeoutError:
        return
    pytest.fail(f"Expected no response, but got {len(data)} bytes: {data!r}")


async def expect_response(loop, sock, timeout: float = METHOD_CALL_TIMEOUT) -> bytes:
    """Return the next datagram that *sock* receives. Fail the test if no datagram arrives in *timeout* seconds."""
    try:
        data, _ = await asyncio.wait_for(loop.sock_recvfrom(sock, 65535), timeout=timeout)
    except asyncio.TimeoutError:
        pytest.fail("Expected a reassembled RESPONSE, but the server sent nothing.")
    return data


async def send_request_and_capture_reply(frame: bytes, host_ip: str, timeout: float, *, port: int):
    """Send *frame* to the ETS server from a new socket bound to *port*. Return the reply and its sender."""
    sock = open_bound_socket(port)
    loop = asyncio.get_running_loop()
    try:
        sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
        return await asyncio.wait_for(loop.sock_recvfrom(sock, 65535), timeout=timeout)
    finally:
        sock.close()


def misdelivered_to(peer_client, session_id: int) -> bool:
    """Return True if *peer_client* received an orphaned response with *session_id*.

    This is direct proof that the reply to the request of the *other* client went to the incorrect client.
    A timeout alone is not proof, because a dropped datagram or a server problem can also cause it.
    """
    return any(resp.session_id == session_id for resp in peer_client.orphaned_responses)


async def send_and_capture_subscribe_ack(
    frame: bytes, host_ip: str, tester_ip: str, timeout: float = NO_RESPONSE_TIMEOUT
):
    """Send a raw SD *frame* to *host_ip*. Return the related SubscribeEventgroupAck/Nack entry, or ``None``.

    The function binds its own socket to (*tester_ip*, ``SD_PORT``).
    It does not use the SD monitor of ``ets_client``, which also uses this port.
    """
    loop = asyncio.get_running_loop()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((tester_ip, SD_PORT))
    sock.setblocking(False)
    try:
        sock.sendto(frame, (host_ip, SD_PORT))
        deadline = loop.time() + timeout
        while loop.time() < deadline:
            remaining = deadline - loop.time()
            try:
                data = await asyncio.wait_for(loop.sock_recv(sock, 65535), timeout=remaining)
            except asyncio.TimeoutError:
                return None
            pkt = SOMEIP(data)
            if pkt.srv_id != 0xFFFF or pkt.sub_id != 0x8100:
                continue
            raw_payload = b"".join(d.load for d in pkt.data if hasattr(d, "load"))
            if not raw_payload:
                continue
            try:
                sd_layer = SD(raw_payload)
            except Exception:  # noqa: BLE001 -- malformed reply; continue to receive
                continue
            for candidate in sd_layer.entry_array or []:
                if (
                    isinstance(candidate, SDEntry_EventGroup)
                    and candidate.type == SDENTRY_TYPE_EVTGRP_SUBSCRIBE_ACK
                    and candidate.eventgroup_id == EVENTGROUP_UNICAST
                ):
                    return candidate
    finally:
        sock.close()
    return None


def udp_endpoint(entry: SdEntry) -> tuple[str, int] | None:
    """Return the address and port of the UDP endpoint option of *entry*, or ``None`` if there is no such option."""
    for option in entry.options_1 + entry.options_2:
        if getattr(option, "l4_proto", None) == L4Proto.UDP:
            return option.addr, option.port
    return None


def udp_endpoint_port(entry: SdEntry) -> int | None:
    """Return the port of the UDP endpoint option of *entry*, or ``None`` if there is no such option."""
    endpoint = udp_endpoint(entry)
    return None if endpoint is None else endpoint[1]
