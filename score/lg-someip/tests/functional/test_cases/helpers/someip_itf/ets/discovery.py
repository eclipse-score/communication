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

"""Makes an ETS client that finds the ETS service of the DUT through SD and receives its replies."""

from __future__ import annotations

import asyncio

import pytest

from someip_itf.ets.client import ScapySomeipClient
from someip_itf.ets.constants import (
    ETS_INSTANCE_ID,
    ETS_MAJOR_VERSION,
    ETS_SERVICE_ID,
    EVENTGROUP_MULTICAST_GROUP,
    SD_MULTICAST_GROUP,
    SERVICE_DISCOVERY_TIMEOUT,
)


async def connect_ets_client(tester_ip: str, client_port: int) -> ScapySomeipClient:
    """Bind a client to ``(tester_ip, client_port)``, find the ETS service, and start the client on the running loop.

    The test fails if the DUT does not offer the service in ``SERVICE_DISCOVERY_TIMEOUT``.
    """
    client = ScapySomeipClient(
        service_id=ETS_SERVICE_ID,
        instance_id=ETS_INSTANCE_ID,
        client_ip=tester_ip,
        client_port=client_port,
        major_version=ETS_MAJOR_VERSION,
        sd_address=SD_MULTICAST_GROUP,
        event_multicast_group=EVENTGROUP_MULTICAST_GROUP,
    )
    try:
        await client.discover_endpoint(timeout=SERVICE_DISCOVERY_TIMEOUT)
    except TimeoutError as exc:
        client.close()
        pytest.fail(
            f"ETS service 0x{ETS_SERVICE_ID:04x}/0x{ETS_INSTANCE_ID:04x} "
            f"not discovered within {SERVICE_DISCOVERY_TIMEOUT:.0f}s: {exc}"
        )
    client.start(asyncio.get_running_loop())
    return client
