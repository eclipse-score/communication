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

"""echoUINT8RELIABLE conformance tests.

echoUINT8RELIABLE is the only method in the ETS interface that uses TCP.
The wire format is the same as echoUINT8: it receives 1 byte and sends the same byte back.
Only the transport is different.
"""

from __future__ import annotations

import asyncio

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import (
    ETS_SERVER_TCP_PORT,
    ETS_SERVICE_ID,
    METHOD_ECHO_UINT8_RELIABLE,
    METHOD_RESET_INTERFACE,
    trigger,
)
from someip_itf.ets.raw import ets_frame
from someip_itf.tcp import tcp_connect, tcp_receive_response

pytestmark = pytest.mark.asyncio(loop_scope="class")

_RESET_DELAY = 0.1
_TCP_TIMEOUT = 5.0


class TestEtsReliable:
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_035"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_035_echo_uint8_reliable_round_trip(self, ets_client, host_ip):  # noqa: ARG002
        """echoUINT8RELIABLE sends back the request byte unchanged over TCP.

        The test uses ets_client only to wait for SD discovery. Thus the raw TCP connect occurs after DUT startup.
        """
        loop = asyncio.get_running_loop()
        sock = await loop.run_in_executor(None, tcp_connect, host_ip, ETS_SERVER_TCP_PORT, _TCP_TIMEOUT)
        try:
            await loop.run_in_executor(
                None, sock.sendall, ets_frame(ETS_SERVICE_ID, METHOD_ECHO_UINT8_RELIABLE, b"\x42")
            )
            result = await loop.run_in_executor(None, tcp_receive_response, sock, _TCP_TIMEOUT)
        finally:
            sock.close()
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert result.payload == b"\x42"

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_037"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_037_reset_interface_does_not_close_tcp_connection(self, ets_client, host_ip):
        """After resetInterface, the open TCP connection is usable for the next request.

        A request sent on the same connection right after the reset gets a normal response.
        """
        loop = asyncio.get_running_loop()
        sock = await loop.run_in_executor(None, tcp_connect, host_ip, ETS_SERVER_TCP_PORT, _TCP_TIMEOUT)
        try:
            await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
            await asyncio.sleep(_RESET_DELAY)

            await loop.run_in_executor(
                None, sock.sendall, ets_frame(ETS_SERVICE_ID, METHOD_ECHO_UINT8_RELIABLE, b"\x99")
            )
            try:
                result = await loop.run_in_executor(None, tcp_receive_response, sock, _TCP_TIMEOUT)
            except (ConnectionError, OSError) as exc:
                pytest.fail(
                    "Expected a normal response on the existing TCP connection "
                    f"after resetInterface; the connection closed instead ({exc})"
                )
        finally:
            sock.close()
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert result.payload == b"\x99"
