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
"""UDP helpers for SOME/IP over UDP (unreliable binding).

The receiver must parse the SOME/IP messages in one UDP datagram one after the other.
It uses only the length field to find the end of each message.
"""

import socket
import time

from someip_itf.codec import SomeipResponse, parse_someip


def udp_send_concatenated(
    sock: socket.socket,
    addr: tuple[str, int],
    messages: list[bytes],
) -> None:
    """Send multiple SOME/IP messages concatenated into ONE UDP datagram.

    The DUT must parse all SOME/IP messages in one UDP datagram.
    This helper joins all *messages* and sends them with one ``sendto()`` call.
    """
    sock.sendto(b"".join(messages), addr)


def udp_receive_responses(
    sock: socket.socket,
    count: int,
    timeout_secs: float = 5.0,
) -> list[SomeipResponse]:
    """Receive exactly *count* SOME/IP responses from a UDP socket.

    All receive calls share one deadline. The total wait is not more than *timeout_secs*.
    Each ``recvfrom()`` call returns one full SOME/IP message.
    The DUT sends one response datagram for each request in the joined datagram.

    Raises ``socket.timeout`` if not all responses arrive in time.
    """
    deadline = time.monotonic() + timeout_secs
    responses: list[SomeipResponse] = []
    while len(responses) < count:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise socket.timeout(f"udp_receive_responses: deadline exceeded after {len(responses)}/{count} responses")
        sock.settimeout(remaining)
        data, _ = sock.recvfrom(65535)
        responses.append(parse_someip(data))
    return responses
