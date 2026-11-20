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

"""Stress test with N unicast subscribers at the same time.

The test makes sure that the SOME/IP stack supports N unicast subscribers.
Each subscriber does these steps:
  1. It subscribes to eventgroup 5. The server confirms the subscription
     with a SubscribeEventgroupAck before the stress period starts.
  2. It receives TestEventUINT8 and TestEventUINT8Array events during a
     30-second stress window. The test triggers the events at a constant rate.

The server sends the SubscribeEventgroupAck (SD entry type 0x07) as unicast to
the SD port (30490) on the host, not to the data port of each client.
On Linux with SO_REUSEADDR (no SO_REUSEPORT), only ONE of the N sockets on that
port receives a unicast datagram. Thus all clients share one ACK counter.
A shared asyncio.Event is set when the server acknowledges all N subscriptions.
"""

from __future__ import annotations

import asyncio
import resource
import types
from typing import List

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.constants import (
    ETS_INSTANCE_ID,
    ETS_MAJOR_VERSION,
    ETS_SERVICE_ID,
    EVENT_UINT8,
    EVENT_UINT8_ARRAY,
    EVENTGROUP_UNICAST,
    METHOD_TRIGGER_EVENT_UINT8,
    METHOD_TRIGGER_EVENT_UINT8_ARRAY,
    SD_MULTICAST_GROUP,
    trigger,
)
from someip_itf.ets.stress import AckCapturingClient

_LGSOMEIP_4_XFAIL_REASON = (
    "[lg-someip #4] the daemon matches a Subscribe to an existing subscription by IP address only, so a second "
    "subscriber endpoint on the same IP address gets no route"
)

_NUM_CLIENTS: int = 200
_BASE_PORT: int = 61000
_MIN_FD_LIMIT: int = _NUM_CLIENTS * 2 + 512
_SUBSCRIBE_ACK_TIMEOUT: float = 30.0
_STRESS_DURATION_S: float = 30.0
_STRESS_WINDOW_TIMEOUT: float = _STRESS_DURATION_S + 10.0
_TRIGGER_INTERVAL_S: float = (
    0.1  # interval between fire-and-forget triggers; it is the same as the 100 ms debounce tick of the ETS server
)
_TEST_TIMEOUT_S: int = int(_SUBSCRIBE_ACK_TIMEOUT + _STRESS_DURATION_S + 30)


class TestSubscriberStress:
    """N unicast subscribers at the same time, each confirmed with a SubscribeAck.

    Change _NUM_CLIENTS to change the number of subscribers.
    """

    @pytest.mark.asyncio(loop_scope="class")
    @pytest.mark.timeout(_TEST_TIMEOUT_S)
    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00453", "AS__PRS_SOMEIPSD_00462"],
        test_type="resource-usage",
        derivation_technique="explorative-testing",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_4_XFAIL_REASON)
    async def test_200_clients_receive_events(self, ets_client, tester_ip) -> None:
        """All N unicast subscribers receive >=90% of the expected events.

        Args:
            ets_client: Class-scoped fixture. It starts the ETS target and
                finds the server endpoint with SD.
            tester_ip:  Address that this test process binds to and joins on.
        """
        loop = asyncio.get_running_loop()

        # Step 1: increase the FD limit.
        soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
        if soft < _MIN_FD_LIMIT:
            resource.setrlimit(resource.RLIMIT_NOFILE, (min(_MIN_FD_LIMIT, hard), hard))

        # Step 2: use the server endpoint that ets_client found.
        server_ip, server_port = ets_client._server
        print(f"\nserver endpoint: {server_ip}:{server_port}")

        # Step 3: set up the shared ACK counter and make the clients.
        all_subscribed = asyncio.Event()
        AckCapturingClient.init_shared_ack_tracking(_NUM_CLIENTS, all_subscribed, loop)

        clients: List[AckCapturingClient] = [
            AckCapturingClient(
                service_id=ETS_SERVICE_ID,
                instance_id=ETS_INSTANCE_ID,
                client_ip=tester_ip,
                client_port=_BASE_PORT + i,
                major_version=ETS_MAJOR_VERSION,
                server_ip=server_ip,
                server_port=server_port,
                sd_address=SD_MULTICAST_GROUP,
            )
            for i in range(_NUM_CLIENTS)
        ]

        eg = types.SimpleNamespace(id=EVENTGROUP_UNICAST)

        try:
            # Step 4a: start the clients and register the callbacks.
            for i, client in enumerate(clients):
                client.start(loop)

                def _cb(method_id: int, _p: bytes, c=client) -> None:
                    if method_id == EVENT_UINT8:
                        c.received_uint8 += 1
                    elif method_id == EVENT_UINT8_ARRAY:
                        c.received_uint8_array += 1

                client.register_callback(_cb)

            # Step 4b: subscribe, with 5 ms between clients.
            for client in clients:
                client.subscribe_eventgroup(eg, ttl=60)
                await asyncio.sleep(0.005)

            # Step 4c: wait for all SubscribeEventgroupAcks on the shared counter.
            print(f"waiting for {_NUM_CLIENTS} SubscribeEventgroupAcks (timeout={_SUBSCRIBE_ACK_TIMEOUT:.0f}s) ...")
            try:
                await asyncio.wait_for(all_subscribed.wait(), timeout=_SUBSCRIBE_ACK_TIMEOUT)
            except asyncio.TimeoutError:
                acked = AckCapturingClient._ack_count[0]
                pytest.fail(
                    f"only {acked}/{_NUM_CLIENTS} SubscribeEventgroupAcks received within {_SUBSCRIBE_ACK_TIMEOUT:.0f}s"
                )
            print(f"all {_NUM_CLIENTS} subscriptions acknowledged.")
            # Make sure that the reader threads are alive before the stress window starts.
            alive = [c._reader_thread.is_alive() for c in clients]
            print(f"reader threads alive: {sum(alive)}/{_NUM_CLIENTS}")

            # Step 5: 30-second stress window.
            print(f"starting {_STRESS_DURATION_S:.0f}s stress window ...")
            trig = AckCapturingClient(
                service_id=ETS_SERVICE_ID,
                instance_id=ETS_INSTANCE_ID,
                client_ip=tester_ip,
                client_port=_BASE_PORT + _NUM_CLIENTS,
                major_version=ETS_MAJOR_VERSION,
                server_ip=server_ip,
                server_port=server_port,
                sd_address=SD_MULTICAST_GROUP,
            )
            trig.start(loop)
            step = 0
            try:

                async def _run_stress() -> int:
                    nonlocal step
                    deadline = loop.time() + _STRESS_DURATION_S
                    while loop.time() < deadline:
                        method = METHOD_TRIGGER_EVENT_UINT8 if step % 2 == 0 else METHOD_TRIGGER_EVENT_UINT8_ARRAY
                        await trigger(trig, method, bytes([0, 1, 1]))
                        await asyncio.sleep(_TRIGGER_INTERVAL_S)
                        step += 1
                    return step

                try:
                    await asyncio.wait_for(_run_stress(), timeout=_STRESS_WINDOW_TIMEOUT)
                except asyncio.TimeoutError:
                    pytest.fail(
                        "stress window did not complete within "
                        f"{_STRESS_WINDOW_TIMEOUT:.0f}s (trigger stall after {step} triggers)"
                    )
            finally:
                trig.close()

            print(
                f"stress window ended — {step} triggers "
                f"({step // 2} TestEventUINT8, {step - step // 2} TestEventUINT8Array)."
            )

            # Let the server send the events in its queue before the checks.
            await asyncio.sleep(2.0)

            # Print the statistics of each client before the checks.
            for i, c in enumerate(clients):
                if c.total_dispatched > 0 or i < 3:
                    print(
                        f"client[{i}] port={_BASE_PORT + i} "
                        f"dispatched={c.total_dispatched} "
                        f"u8={c.received_uint8} arr={c.received_uint8_array} "
                        f"methods={c.seen_method_ids}"
                    )
            print(
                "reader threads still alive after stress: "
                f"{sum(c._reader_thread.is_alive() for c in clients)}/{_NUM_CLIENTS}"
            )

            # Step 6: make sure that all N clients received >=90% of the expected events.
            expected_u8 = step // 2
            expected_arr = step - step // 2
            min_u8 = int(expected_u8 * 0.9)
            min_arr = int(expected_arr * 0.9)

            missing_u8 = [i for i, c in enumerate(clients) if c.received_uint8 < min_u8]
            missing_arr = [i for i, c in enumerate(clients) if c.received_uint8_array < min_arr]

            if missing_u8:
                worst = min(clients[i].received_uint8 for i in missing_u8)
                pytest.fail(
                    f"{len(missing_u8)}/{_NUM_CLIENTS} clients received "
                    f"<{min_u8}/{expected_u8} TestEventUINT8 (worst={worst}) — "
                    f"ports: {[_BASE_PORT + i for i in missing_u8[:10]]}"
                )
            if missing_arr:
                worst = min(clients[i].received_uint8_array for i in missing_arr)
                pytest.fail(
                    f"{len(missing_arr)}/{_NUM_CLIENTS} clients received "
                    f"<{min_arr}/{expected_arr} TestEventUINT8Array (worst={worst}) — "
                    f"ports: {[_BASE_PORT + i for i in missing_arr[:10]]}"
                )

            print(
                f"PASS — all {_NUM_CLIENTS} clients received "
                f">=90% of expected events "
                f"(>={min_u8}/{expected_u8} UINT8, >={min_arr}/{expected_arr} UINT8Array)."
            )

        finally:
            for client in clients:
                client.close()
