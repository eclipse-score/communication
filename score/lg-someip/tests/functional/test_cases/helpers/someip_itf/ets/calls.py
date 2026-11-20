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

"""Calls to the ETS service and checks of its answers. All ETS suites use them."""

from __future__ import annotations

import asyncio

from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import (
    METHOD_CLIENT_SERVICE_CALL_ECHO_UINT8_ARRAY,
    METHOD_CLIENT_SERVICE_GET_LAST_VALUE_MULTICAST,
    METHOD_CLIENT_SERVICE_GET_LAST_VALUE_UNICAST,
    METHOD_ECHO_BITFIELDS,
    METHOD_ECHO_UINT8,
    METHOD_FIELD_UINT8_GET,
    METHOD_SUSPEND_INTERFACE,
    call,
    trigger,
)
from someip_itf.ets.payloads import bitfields_request, bitfields_response, pack_array

PROXY_CALL_TIMEOUT = 30.0  # the server polls the proxy for a maximum of 10 s; this adds a margin
#: Interval between two ``kill -0`` polls on the target.
EXIT_POLL_INTERVAL: float = 0.1


def assert_ok(result) -> None:
    """Make sure that *result* is a RESPONSE with E_OK."""
    assert result.message_type == MessageType.RESPONSE
    assert result.return_code == ReturnCode.E_OK


async def assert_dut_alive_after_timeout(ets_client) -> None:
    """After a request timeout, make sure that the DUT still answers a simple echoUINT8 call."""
    result = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
    assert result.message_type == MessageType.RESPONSE
    assert result.return_code == ReturnCode.E_OK


async def wait_for_availability(client, want: bool, timeout: float, poll: float = 0.1) -> float:
    """Poll is_available() until it is equal to `want`. Return the elapsed seconds.

    Raise asyncio.TimeoutError if `timeout` expires first.
    """
    deadline = asyncio.get_running_loop().time() + timeout
    t0 = asyncio.get_running_loop().time()
    while asyncio.get_running_loop().time() < deadline:
        if await client.is_available() == want:
            return asyncio.get_running_loop().time() - t0
        await asyncio.sleep(poll)
    raise asyncio.TimeoutError(f"is_available() did not become {want} within {timeout:.1f} s")


async def fire_suspend(client, start: int, duration: int) -> None:
    """Send suspendInterface(start, duration) as REQUEST_NO_RETURN."""
    await trigger(client, METHOD_SUSPEND_INTERFACE, bytes([start, duration]))


async def get_field_uint8(client) -> int:
    """Read TestFieldUINT8 with its getter. Make sure that the response is E_OK with one byte."""
    result = await call(client, METHOD_FIELD_UINT8_GET, b"")
    assert result.message_type == MessageType.RESPONSE
    assert result.return_code == ReturnCode.E_OK
    assert len(result.payload) == 1, f"getter response payload {result.payload.hex()} is not one uint8"
    return result.payload[0]


async def check_walking_one(client, width: int, bit_pos: int) -> None:
    """Send one set bit at *bit_pos* of the *width*-bit field. Make sure that it returns mirrored, other fields 0."""
    sent = {8: 0, 16: 0, 32: 0}
    sent[width] = 1 << bit_pos
    result = await call(client, METHOD_ECHO_BITFIELDS, bitfields_request(sent[8], sent[16], sent[32]))
    assert result.message_type == MessageType.RESPONSE
    assert result.return_code == ReturnCode.E_OK
    assert len(result.payload) == 7
    received = dict(zip((8, 16, 32), bitfields_response(result.payload)))
    for field_width, got in received.items():
        expected = 1 << (width - 1 - bit_pos) if field_width == width else 0
        digits = field_width // 4
        assert got == expected, (
            f"bit_pos={bit_pos} in bitfield{width}_in: "
            f"bitfield{field_width}_out 0x{got:0{digits}x} != expected 0x{expected:0{digits}x}"
        )


async def call_echo(client, values: list[int]) -> list[int]:
    """Call the uint8 array echo of the client service through the DUT proxy and return the response."""
    result = await asyncio.wait_for(
        client.call_method(METHOD_CLIENT_SERVICE_CALL_ECHO_UINT8_ARRAY, pack_array(values)),
        timeout=PROXY_CALL_TIMEOUT,
    )
    return result


async def get_last_value_unicast(client) -> int:
    """Read the last value that the DUT proxy received on the unicast event."""
    result = await asyncio.wait_for(
        client.call_method(METHOD_CLIENT_SERVICE_GET_LAST_VALUE_UNICAST, b""),
        timeout=10.0,
    )
    return result


async def get_last_value_multicast(client) -> int:
    """Read the last value that the DUT proxy received on the multicast event."""
    result = await asyncio.wait_for(
        client.call_method(METHOD_CLIENT_SERVICE_GET_LAST_VALUE_MULTICAST, b""),
        timeout=10.0,
    )
    return result


async def poll_until_exited(target, pid: int, timeout: float) -> float | None:
    """Poll ``kill -0 <pid>`` until the process stops or *timeout* expires.

    Return the elapsed seconds until the process stops, or ``None`` on timeout.
    """
    loop = asyncio.get_running_loop()
    t_start = loop.time()
    deadline = t_start + timeout
    while loop.time() < deadline:
        exit_code, _ = await loop.run_in_executor(None, target.execute, f"kill -0 {pid} 2>/dev/null")
        if exit_code != 0:
            return loop.time() - t_start
        await asyncio.sleep(EXIT_POLL_INTERVAL)
    return None
