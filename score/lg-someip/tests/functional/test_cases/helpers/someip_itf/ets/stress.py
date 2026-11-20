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

"""ETS client for the stress suite. It counts the SubscribeEventgroupAcks of many clients on one SD port."""

from __future__ import annotations

import asyncio
import socket
import threading

from scapy.contrib.automotive.someip import SD, SDENTRY_TYPE_EVTGRP_SUBSCRIBE_ACK, SOMEIP, SDEntry_EventGroup

from someip_itf.ets.client import _SOMEIP_HEADER_SIZE, ScapySomeipClient
from someip_itf.ets.constants import EVENTGROUP_UNICAST


class AckCapturingClient(ScapySomeipClient):
    """ScapySomeipClient that finds SubscribeEventgroupAck messages with its SD monitor.

    The DUT sends SubscribeEventgroupAck messages unicast to the SD port (30490) of the host.
    All clients have SD monitor sockets on this port (SO_REUSEADDR).
    On Linux, only one of these sockets receives a unicast datagram.
    Thus all clients use one shared counter and one shared asyncio.Event.
    The _sd_monitor_loop of each client increments the counter for each ACK.
    When the counter gets to the target from init_shared_ack_tracking, the loop sets the shared event.
    """

    # Shared class state. init_shared_ack_tracking() sets it.
    _ack_lock: threading.Lock | None = None
    _ack_count: list | None = None  # mutable [int], so that threads can change it
    _ack_target: int = 0
    _ack_asyncio_event: asyncio.Event | None = None
    _ack_event_loop: asyncio.AbstractEventLoop | None = None
    _ack_eg_id: int = EVENTGROUP_UNICAST

    @classmethod
    def init_shared_ack_tracking(
        cls,
        n: int,
        ev: asyncio.Event,
        loop: asyncio.AbstractEventLoop,
    ) -> None:
        """Set the shared ACK counter state before you make the clients.

        Call it one time from the running event loop, before you start a client.
        """
        cls._ack_lock = threading.Lock()
        cls._ack_count = [0]
        cls._ack_target = n
        cls._ack_asyncio_event = ev
        cls._ack_event_loop = loop

    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)
        self.received_uint8: int = 0
        self.received_uint8_array: int = 0
        self.total_dispatched: int = 0  # debug: number of _dispatch calls
        self.seen_method_ids: list = []  # debug: first few method IDs

    def _dispatch(self, session_id: int, raw: bytes) -> None:  # type: ignore[override]
        """Count all received frames for debug, then dispatch them."""
        self.total_dispatched += 1
        if len(self.seen_method_ids) < 5 and len(raw) >= _SOMEIP_HEADER_SIZE:
            from scapy.contrib.automotive.someip import SOMEIP as _SOMEIP

            try:
                mid = _SOMEIP(raw).sub_id
                mt = int(_SOMEIP(raw).msg_type)
                self.seen_method_ids.append((mid, mt))
            except Exception:
                pass
        super()._dispatch(session_id, raw)

    def _sd_monitor_loop(self) -> None:
        """Find SubscribeEventgroupAck messages only. Do not subscribe again on OfferService.

        The SD monitor of the base class subscribes again on each OfferService.
        With N clients on the same multicast group, each offer causes N call_soon_threadsafe calls.
        Each call builds a Scapy packet (approximately 1-10 ms).
        This blocks the asyncio event loop, and timer callbacks do not run in time.
        Then asyncio.sleep(0.005) and wait_for timeouts do not expire.
        The stagger loop then needs approximately 1 s for each step.

        The stress clients expect a stable server. Thus they must only find the ACKs.
        """
        while self._running:
            try:
                data, _ = self._sd_monitor_sock.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError:
                break
            if len(data) < _SOMEIP_HEADER_SIZE:
                continue
            pkt = SOMEIP(data)
            if pkt.srv_id != 0xFFFF or pkt.sub_id != 0x8100:
                continue
            raw_payload = b"".join(d.load for d in pkt.data if hasattr(d, "load"))
            if not raw_payload:
                continue
            try:
                sd_layer = SD(raw_payload)
            except Exception:
                continue
            for entry in sd_layer.entry_array or []:
                # Find the SubscribeEventgroupAck of this client in all SD traffic on the shared monitor socket.
                if (
                    isinstance(entry, SDEntry_EventGroup)
                    and entry.type == SDENTRY_TYPE_EVTGRP_SUBSCRIBE_ACK
                    and entry.srv_id == self._service_id
                    and entry.inst_id == self._instance_id
                    and entry.eventgroup_id == self._ack_eg_id
                    and entry.ttl > 0
                    and self._ack_lock is not None
                ):
                    with self._ack_lock:
                        self._ack_count[0] += 1
                        reached = self._ack_count[0] >= self._ack_target
                    if reached and not self._ack_asyncio_event.is_set():
                        self._ack_event_loop.call_soon_threadsafe(self._ack_asyncio_event.set)
                    break
