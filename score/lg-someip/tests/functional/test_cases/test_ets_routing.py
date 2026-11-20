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

"""Instance routing collision test.

Another ECU offers the same service ID with a different instance on the same UDP port.
The test makes sure that method calls to the local instance still work.

Test setup:
  [Python test] --(SOME/IP UDP + crafted SD Offer)--> [someipd] --(UDS)--> [ets]

ETS offers service 0x0103 / instance 0x0091. The test sends a competing remote
OfferService for 0x0103 / instance 0x00F4 on the same UDP port. Then it calls a
method and makes sure that a response comes back.
"""

from __future__ import annotations

import asyncio
import struct

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import METHOD_ECHO_UINT8, call

pytestmark = pytest.mark.asyncio(loop_scope="class")

# Competing remote instance id (≠ ETS_INSTANCE_ID 0x0091).
_COMPETING_INSTANCE_ID = 0x00F4
# An endpoint address that looks like a remote tester host.
_COMPETING_REMOTE_IP = "160.48.199.64"


class TestEtsInstanceRoutingCollision:
    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIP_00162"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    async def test_method_responds_despite_competing_remote_offer_same_port(self, ets_client):
        """Method calls to the local instance work when another ECU offers the same service.

        The remote offer has a different instance and the same port.
        """
        # ets_client found ETS 0x0103/0x0091. The daemon has the local
        # (service, server-port) -> local-instance mapping.
        server_port = ets_client.server_port
        assert server_port is not None, "ETS server endpoint port was not discovered"

        # Send a competing remote OfferService: the same service ID, a different
        # instance, and the SAME UDP port number as the local service.
        for _ in range(3):
            ets_client.send_sd_offer(
                instance_id=_COMPETING_INSTANCE_ID,
                ip=_COMPETING_REMOTE_IP,
                port=server_port,
            )
            await asyncio.sleep(0.3)
        # Give the daemon time to process the remote offer.
        await asyncio.sleep(1.0)

        # The local ETS service must answer a normal method call.
        payload = struct.pack(">B", 0x2A)
        try:
            result = await call(ets_client, METHOD_ECHO_UINT8, payload)
        except asyncio.TimeoutError:
            pytest.fail(
                "No response to echoUINT8: a competing remote SD Offer for the "
                f"same service on port {server_port} changed the instance routing "
                "and the daemon dropped the request."
            )

        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert result.payload[:1] == payload
