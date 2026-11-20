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
"""Helpers that send field GET and SET requests over UDP and TCP for the TC8 field tests."""

from typing import List, Sequence

from someip_itf.codec import SomeipResponse, parse_someip
from someip_itf.tc8.message_builder import build_request
from someip_itf.tc8.sd import create_udp_socket
from someip_itf.tc8.udp import udp_receive_responses
from someip_itf.tcp import tcp_connect, tcp_receive_response, tcp_send_request

#: The DUT field dispatch discards a request if its interface_version is not the major version of the deployment.
#: It sends no error response. The method dispatch sends E_WRONG_INTERFACE_VERSION for the same error.
#: Thus, all field calls use the real major version, not the build_request default of 0.
_FIELD_MAJOR_VERSION: int = 0x01


def send_get_field(
    host_ip: str,
    service_id: int,
    get_method_id: int,
    dut_port: int,
    client_id: int = 0x0020,
    session_id: int = 0x0001,
    timeout_secs: float = 3.0,
    interface_version: int = _FIELD_MAJOR_VERSION,
) -> SomeipResponse:
    """Send a GET field request and return the RESPONSE.

    Raises ``socket.timeout`` if no response arrives within *timeout_secs*.
    """
    request_bytes = build_request(
        service_id,
        get_method_id,
        client_id=client_id,
        session_id=session_id,
        interface_version=interface_version,
    )
    sock = create_udp_socket(port=0)
    try:
        sock.sendto(request_bytes, (host_ip, dut_port))
        sock.settimeout(timeout_secs)
        data, _ = sock.recvfrom(65535)
        return parse_someip(data)
    finally:
        sock.close()


def send_set_field(
    host_ip: str,
    service_id: int,
    set_method_id: int,
    new_value: bytes,
    dut_port: int,
    client_id: int = 0x0020,
    session_id: int = 0x0002,
    timeout_secs: float = 3.0,
    interface_version: int = _FIELD_MAJOR_VERSION,
) -> SomeipResponse:
    """Send a SET field request with *new_value* as payload and return the RESPONSE.

    Raises ``socket.timeout`` if no response arrives within *timeout_secs*.
    """
    request_bytes = build_request(
        service_id,
        set_method_id,
        client_id=client_id,
        session_id=session_id,
        payload=new_value,
        interface_version=interface_version,
    )
    sock = create_udp_socket(port=0)
    try:
        sock.sendto(request_bytes, (host_ip, dut_port))
        sock.settimeout(timeout_secs)
        data, _ = sock.recvfrom(65535)
        return parse_someip(data)
    finally:
        sock.close()


def send_set_fields_pipelined(
    host_ip: str,
    service_id: int,
    set_method_id: int,
    values: Sequence[bytes],
    dut_port: int,
    client_id: int = 0x0021,
    first_session_id: int = 0x0100,
    timeout_secs: float = 5.0,
    interface_version: int = _FIELD_MAJOR_VERSION,
) -> List[SomeipResponse]:
    """Send one SET for each value, then receive the same number of RESPONSEs.

    The helper sends all SETs before it reads a reply.
    The session IDs start at *first_session_id* and increase by one for each value.
    The replies are in the order of arrival. The payload of each reply is the value that its handler accepted.
    Raises ``socket.timeout`` if fewer than ``len(values)`` replies arrive within *timeout_secs*.
    """
    requests = [
        build_request(
            service_id,
            set_method_id,
            client_id=client_id,
            session_id=first_session_id + index,
            payload=value,
            interface_version=interface_version,
        )
        for index, value in enumerate(values)
    ]
    sock = create_udp_socket(port=0)
    try:
        for request in requests:
            sock.sendto(request, (host_ip, dut_port))
        return udp_receive_responses(sock, count=len(requests), timeout_secs=timeout_secs)
    finally:
        sock.close()


# ---------------------------------------------------------------------------
# TCP (reliable transport)
# ---------------------------------------------------------------------------


def send_get_field_tcp(
    host_ip: str,
    service_id: int,
    get_method_id: int,
    dut_port: int,
    client_id: int = 0x0040,
    session_id: int = 0x0010,
    timeout_secs: float = 3.0,
    interface_version: int = _FIELD_MAJOR_VERSION,
) -> SomeipResponse:
    """Send a GET field request over TCP and return the RESPONSE.

    This is the TCP version of send_get_field().
    """
    request_bytes = build_request(
        service_id,
        get_method_id,
        client_id=client_id,
        session_id=session_id,
        interface_version=interface_version,
    )
    sock = tcp_connect(host_ip, dut_port, timeout_secs=timeout_secs)
    try:
        tcp_send_request(sock, request_bytes)
        return tcp_receive_response(sock, timeout_secs=timeout_secs)
    finally:
        sock.close()


def send_set_field_tcp(
    host_ip: str,
    service_id: int,
    set_method_id: int,
    new_value: bytes,
    dut_port: int,
    client_id: int = 0x0040,
    session_id: int = 0x0011,
    timeout_secs: float = 3.0,
    interface_version: int = _FIELD_MAJOR_VERSION,
) -> SomeipResponse:
    """Send a SET field request over TCP with *new_value* and return the RESPONSE.

    This is the TCP version of send_set_field().
    """
    request_bytes = build_request(
        service_id,
        set_method_id,
        client_id=client_id,
        session_id=session_id,
        payload=new_value,
        interface_version=interface_version,
    )
    sock = tcp_connect(host_ip, dut_port, timeout_secs=timeout_secs)
    try:
        tcp_send_request(sock, request_bytes)
        return tcp_receive_response(sock, timeout_secs=timeout_secs)
    finally:
        sock.close()
