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
"""TCP transport helpers for SOME/IP over TCP (reliable binding).

SOME/IP over TCP uses stream framing. The 4-byte length field in the header
(bytes 4–7) gives the number of bytes after byte 7. The receiver reads
8 bytes (service_id + method_id + length). Then it reads `length` more bytes.
"""

import socket
import struct
import time

from someip_itf.codec import SomeipResponse, parse_someip

_MAX_SOMEIP_MSG_SIZE: int = 65536  # 64 KB safety limit


def _recv_exact(sock: socket.socket, nbytes: int, deadline: float) -> bytes:
    """Read exactly *nbytes* from *sock*. Read again after a partial read.

    Raises socket.timeout if the deadline passes before all bytes arrive.
    Raises ConnectionError if the peer closes the connection.
    """
    buf = bytearray()
    while len(buf) < nbytes:
        remaining_time = deadline - time.monotonic()
        if remaining_time <= 0:
            raise socket.timeout("tcp_receive_response: deadline exceeded during recv")
        sock.settimeout(remaining_time)
        chunk = sock.recv(nbytes - len(buf))
        if not chunk:
            raise ConnectionError("TCP peer closed connection before all bytes received")
        buf.extend(chunk)
    return bytes(buf)


def tcp_connect(host_ip: str, port: int, timeout_secs: float = 5.0) -> socket.socket:
    """Open a TCP connection to the DUT.

    Returns the connected socket. The caller must close it.
    Raises ConnectionRefusedError or socket.timeout on failure.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout_secs)
    sock.connect((host_ip, port))
    return sock


def tcp_send_request(sock: socket.socket, request_bytes: bytes) -> None:
    """Send a complete SOME/IP message over a TCP connection.

    Uses sendall() to make sure that all bytes are sent.
    """
    sock.sendall(request_bytes)


def tcp_send_concatenated(sock: socket.socket, messages: list[bytes]) -> None:
    """Send many SOME/IP messages in a single TCP write.

    SOME/IP requires that a TCP receiver accepts many SOME/IP
    messages in a single TCP segment (unaligned packing).
    This helper joins all *messages* and sends them with one
    ``sendall()`` call, so the DUT receives them in one segment.
    """
    sock.sendall(b"".join(messages))


def tcp_receive_n_responses(
    sock: socket.socket,
    count: int,
    timeout_secs: float = 5.0,
) -> list[SomeipResponse]:
    """Receive exactly *count* SOME/IP responses from a TCP stream.

    All *count* receive calls use one shared deadline, so the
    total wait is not more than *timeout_secs*. Raises ``socket.timeout`` if
    not all responses arrive in time.
    """
    deadline = time.monotonic() + timeout_secs
    responses: list[SomeipResponse] = []
    while len(responses) < count:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise socket.timeout(f"tcp_receive_n_responses: deadline exceeded after {len(responses)}/{count} responses")
        responses.append(tcp_receive_response(sock, timeout_secs=remaining))
    return responses


def tcp_receive_response(sock: socket.socket, timeout_secs: float = 3.0) -> SomeipResponse:
    """Receive one complete SOME/IP message from a TCP stream.

    Framing:
      1. Read 8 bytes (service_id[2] + method_id[2] + length[4]).
      2. Extract length from bytes 4–7 (big-endian).
      3. Read exactly `length` more bytes.
      4. Parse via parse_someip(header + body).

    Raises socket.timeout if the complete message does not arrive in time.
    Raises ValueError if length is more than the 64 KB safety limit.
    """
    deadline = time.monotonic() + timeout_secs
    header_prefix = _recv_exact(sock, 8, deadline)
    length = struct.unpack("!I", header_prefix[4:8])[0]
    if length > _MAX_SOMEIP_MSG_SIZE:
        raise ValueError(f"SOME/IP length field {length} exceeds safety bound {_MAX_SOMEIP_MSG_SIZE}")
    body = _recv_exact(sock, length, deadline)
    return parse_someip(header_prefix + body)


def tcp_listen(host_ip: str, port: int = 0, backlog: int = 1) -> socket.socket:
    """Create a TCP server socket and start listening.

    If *port* is 0, the OS gives an ephemeral port. Use
    ``sock.getsockname()[1]`` to get this port.

    Returns the listening socket. The caller must close it.
    """
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((host_ip, port))
    srv.listen(backlog)
    return srv


def tcp_accept_and_receive_notification(
    srv_sock: socket.socket,
    event_id: int,
    service_id: int,
    timeout_secs: float = 8.0,
) -> SomeipResponse:
    """Accept a TCP connection and receive one SOME/IP notification.

    The DUT connects to the listening socket to send event notifications.
    Uses the same SOME/IP TCP framing as tcp_receive_response().

    Returns the first notification matching *service_id* and *event_id*.
    Raises socket.timeout or AssertionError if no matching notification arrives.
    """
    srv_sock.settimeout(timeout_secs)
    conn, _ = srv_sock.accept()
    try:
        deadline = time.monotonic() + timeout_secs
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            header_prefix = _recv_exact(conn, 8, deadline)
            length = struct.unpack("!I", header_prefix[4:8])[0]
            if length > _MAX_SOMEIP_MSG_SIZE:
                raise ValueError(f"SOME/IP length field {length} exceeds safety bound {_MAX_SOMEIP_MSG_SIZE}")
            body = _recv_exact(conn, length, deadline)
            msg = parse_someip(header_prefix + body)
            if msg.service_id == service_id and msg.method_id == event_id:
                return msg
        raise socket.timeout(
            f"No SOME/IP notification for service 0x{service_id:04x} "
            f"event 0x{event_id:04x} received via TCP within {timeout_secs}s"
        )
    finally:
        conn.close()
