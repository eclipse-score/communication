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

"""ETS instance of the tester. The ETS client mode of the DUT finds it, subscribes to it, and calls it."""

from __future__ import annotations

import asyncio
import contextlib
import socket
import threading
import time
from collections.abc import Callable

from scapy.contrib.automotive.someip import SDOption_IP4_EndPoint, SDOption_IP4_Multicast

from someip_itf.codec import (
    L4Proto,
    MalformedSomeip,
    MessageType,
    SdEntry,
    SdEntryType,
    SomeipResponse,
    build_sd,
    build_someip,
    parse_sd_datagram,
    parse_someip,
)
from someip_itf.ets.constants import (
    ETS_MAJOR_VERSION,
    ETS_MINOR_VERSION,
    ETS_REQUIRED_INSTANCE_ID,
    ETS_SERVICE_ID,
    SD_MULTICAST_GROUP,
    SD_PORT,
)
from someip_itf.ets.sd import close_sd_multicast_socket, open_sd_multicast_socket

_OFFER_INTERVAL = 1.0  # seconds between cyclic offers
_OFFER_TTL = 3  # offer entry TTL; > _OFFER_INTERVAL, so that the service is always available
_DATA_POLL = 0.2  # time that the worker waits on the data socket before it checks if an offer is due
_REBOOT_SILENCE = 0.5  # no offer in this time before a reboot, so that a Subscribe to the last offer arrives first
_RECEIVE_SIZE = 65535

EntryPredicate = Callable[[SdEntry], bool]


def _open_unicast_socket(tester_ip: str, port: int) -> socket.socket:
    """Open a UDP socket bound to ``(tester_ip, port)`` that sends multicast on *tester_ip*. Close it if bind fails."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # Without it, a datagram to a group uses the default route, not the link to the DUT.
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(tester_ip))
        sock.bind((tester_ip, port))
    except OSError:
        sock.close()
        raise
    return sock


def _is_tester_ets_entry(entry: SdEntry) -> bool:
    return entry.service_id == ETS_SERVICE_ID and entry.instance_id == ETS_REQUIRED_INSTANCE_ID


def _read_available(sock: socket.socket) -> list[bytes]:
    """Return all datagrams that are in the queue of the non-blocking *sock*."""
    received = []
    while True:
        try:
            received.append(sock.recv(_RECEIVE_SIZE))
        except BlockingIOError:
            return received


class _SessionCounter:
    """Gives SOME/IP session IDs: 1 to 0xFFFF, then 1 again."""

    def __init__(self) -> None:
        self._next = 1

    def take(self) -> int:
        session_id = self._next
        self._next = self._next % 0xFFFF + 1
        return session_id

    def restart(self) -> None:
        self._next = 1


class TesterEts:
    """Offers ETS ``ETS_SERVICE_ID``/``ETS_REQUIRED_INSTANCE_ID`` from the tester. Sends an echo to each REQUEST.

    While this object is open, it receives the unicast SD to *tester_ip*, not the SD monitor of ``ets_client``.
    Thus a suite that uses both must not expect a SubscribeAck or a unicast Offer on ``ets_client``.
    someipd identifies SD peers by address. Thus this object and ``ets_client`` are one peer for the DUT.
    Each new instance starts at session 1 with the Reboot flag set. The DUT sees this as a reboot of *tester_ip*.
    The object reads FindService from the SD multicast group.
    It reads Subscribe and StopSubscribe from ``(tester_ip, SD_PORT)``.
    """

    def __init__(self, tester_ip: str, dut_ip: str, udp_port: int) -> None:
        self._tester_ip = tester_ip
        self._dut_ip = dut_ip
        self._udp_port = udp_port
        # The REQUESTs answered since the start or the last reboot().
        self.requests: list[SomeipResponse] = []
        # Entries of a datagram that is read but not yet requested. One SD datagram can have many entries.
        self._pending: dict[socket.socket, list[SdEntry]] = {}
        self._session_lock = threading.Lock()
        self._multicast_sessions = _SessionCounter()
        self._unicast_sessions = _SessionCounter()
        self._event_sessions = _SessionCounter()
        self._offering = True
        self._halt = threading.Event()
        self._worker: threading.Thread | None = None
        self._worker_error: BaseException | None = None

        with contextlib.ExitStack() as opened:
            self._sd = opened.enter_context(_open_unicast_socket(tester_ip, SD_PORT))
            self._sd.setblocking(False)
            self._sd_multicast, self._mreq = open_sd_multicast_socket(tester_ip)
            opened.callback(close_sd_multicast_socket, self._sd_multicast, self._mreq)
            self._data = opened.enter_context(_open_unicast_socket(tester_ip, udp_port))
            self._data.settimeout(_DATA_POLL)
            opened.pop_all()
        # The old data sockets from reboot(). They stay bound, so the tester receives what the DUT sends to an old port.
        self._retired: list[socket.socket] = []

    def start(self) -> None:
        """Start a background thread that sends an offer each ``_OFFER_INTERVAL`` and answers REQUESTs."""
        self._halt.clear()
        self._worker = threading.Thread(target=self._run, daemon=True, name="tester-ets")
        self._worker.start()

    def stop_offer(self) -> None:
        """Send one StopOffer, then stop offers and answers. The SD sockets stay open, so reads continue to work."""
        self._halt_worker()
        self._offering = False
        self._send_offer(ttl=0)

    def stop(self) -> None:
        """Send a StopOffer if none was sent. Close all sockets, also if the send fails."""
        try:
            self._halt_worker()
            if self._offering:
                with contextlib.suppress(OSError):
                    self._send_offer(ttl=0)
        finally:
            for sock in self._retired:
                sock.close()
            self._data.close()
            self._sd.close()
            close_sd_multicast_socket(self._sd_multicast, self._mreq)

    def reboot(self, udp_port: int) -> None:
        """Simulate a reboot: set the SD session counters to 1 again and offer the data endpoint on *udp_port*.

        No StopOffer is sent first, as from a peer that lost power.
        The DUT finds the reboot because the session ID decreases and the Reboot flag is set.
        The function discards SD traffic from before the reboot. The old data port stays bound for ``retired_traffic``.
        It blocks for ``_REBOOT_SILENCE`` s. Thus call it through ``asyncio.to_thread`` from a coroutine.
        """
        self._halt_worker()
        time.sleep(_REBOOT_SILENCE)
        self.drain()
        self._data.setblocking(False)
        self._retired.append(self._data)
        with self._session_lock:
            self._multicast_sessions.restart()
            self._unicast_sessions.restart()
        self.requests = []
        self._udp_port = udp_port
        self._data = _open_unicast_socket(self._tester_ip, udp_port)
        self._data.settimeout(_DATA_POLL)
        self._offering = True
        self.start()

    def retired_traffic(self) -> list[bytes]:
        """Return the datagrams that the DUT sent to an old data port after ``reboot``, since the last call."""
        received = []
        for sock in self._retired:
            received.extend(_read_available(sock))
        return received

    def drain(self) -> None:
        """Discard the received SD traffic. The next read gets only what the DUT sends after this call."""
        for sock in (self._sd, self._sd_multicast):
            self._pending.pop(sock, None)
            _read_available(sock)

    async def wait_for(
        self, entry_type: SdEntryType, timeout: float, predicate: EntryPredicate | None = None
    ) -> SdEntry | None:
        """Return the next *entry_type* entry that the DUT sends for the tester ETS, or ``None`` after *timeout* s.

        A StopSubscribe is a Subscribe entry with TTL 0. Use a predicate on ``ttl`` to find the difference.
        """
        found = await self.collect(entry_type, timeout, predicate, limit=1)
        return found[0] if found else None

    async def collect(
        self,
        entry_type: SdEntryType,
        window: float,
        predicate: EntryPredicate | None = None,
        limit: int | None = None,
    ) -> list[SdEntry]:
        """Return all matching entries that the DUT sends in *window* s, or only the first *limit* entries.

        Entries that do not match stay in the queue for a later call.
        Raise ``RuntimeError`` if the background thread stopped with an error.
        """
        sock = self._sd_multicast if entry_type == SdEntryType.FindService else self._sd
        loop = asyncio.get_running_loop()
        deadline = loop.time() + window
        found: list[SdEntry] = []
        skipped: list[SdEntry] = []
        pending = self._pending.setdefault(sock, [])
        try:
            while limit is None or len(found) < limit:
                self._raise_worker_error()
                if not pending:
                    remaining = deadline - loop.time()
                    if remaining <= 0:
                        break
                    try:
                        data = await asyncio.wait_for(loop.sock_recv(sock, _RECEIVE_SIZE), timeout=remaining)
                    except asyncio.TimeoutError:
                        break
                    parsed = parse_sd_datagram(data)
                    if parsed is not None:
                        pending.extend(parsed[1].entries)
                    continue
                entry = pending.pop(0)
                if (
                    entry.sd_type == entry_type
                    and _is_tester_ets_entry(entry)
                    and (predicate is None or predicate(entry))
                ):
                    found.append(entry)
                else:
                    skipped.append(entry)
        finally:
            pending[:0] = skipped
        return found

    def ack(self, subscribe: SdEntry, multicast_group: tuple[str, int] | None = None) -> None:
        """Send an Ack for *subscribe* to the DUT. With *multicast_group*, the Ack gives that group for the events."""
        options = ()
        if multicast_group is not None:
            group_ip, group_port = multicast_group
            options = (SDOption_IP4_Multicast(addr=group_ip, l4_proto=L4Proto.UDP, port=group_port),)
        entry = SdEntry(
            sd_type=SdEntryType.SubscribeAck,
            service_id=subscribe.service_id,
            instance_id=subscribe.instance_id,
            major_version=subscribe.major_version,
            ttl=subscribe.ttl,
            minver_or_counter=subscribe.minver_or_counter,
            options_1=options,
        )
        with self._session_lock:
            session_id = self._unicast_sessions.take()
        self._sd.sendto(build_sd([entry], session_id=session_id, flag_reboot=True), (self._dut_ip, SD_PORT))

    def send_event(self, event_id: int, payload: bytes, destination: tuple[str, int]) -> None:
        """Send one NOTIFICATION of *event_id* from the offered data endpoint to *destination*."""
        self._data.sendto(
            build_someip(
                service_id=ETS_SERVICE_ID,
                method_id=event_id,
                client_id=0,
                session_id=self._event_sessions.take(),
                interface_version=ETS_MAJOR_VERSION,
                message_type=MessageType.NOTIFICATION,
                payload=payload,
            ),
            destination,
        )

    def _raise_worker_error(self) -> None:
        if self._worker_error is not None:
            raise RuntimeError("the tester ETS's background thread died") from self._worker_error

    def _halt_worker(self) -> None:
        self._halt.set()
        if self._worker is not None:
            self._worker.join(timeout=2.0)
            if self._worker.is_alive():
                raise RuntimeError("the tester ETS's background thread did not stop")
            self._worker = None
        self._raise_worker_error()

    def _send_offer(self, ttl: int) -> None:
        entry = SdEntry(
            sd_type=SdEntryType.OfferService,
            service_id=ETS_SERVICE_ID,
            instance_id=ETS_REQUIRED_INSTANCE_ID,
            major_version=ETS_MAJOR_VERSION,
            ttl=ttl,
            minver_or_counter=ETS_MINOR_VERSION,
            options_1=(SDOption_IP4_EndPoint(addr=self._tester_ip, l4_proto=L4Proto.UDP, port=self._udp_port),),
        )
        with self._session_lock:
            session_id = self._multicast_sessions.take()
        self._sd.sendto(build_sd([entry], session_id=session_id, flag_reboot=True), (SD_MULTICAST_GROUP, SD_PORT))

    def _answer(self, data: bytes, sender: tuple[str, int]) -> None:
        try:
            request = parse_someip(data)
        except MalformedSomeip:
            return
        if request.service_id != ETS_SERVICE_ID or request.message_type != MessageType.REQUEST:
            return
        self.requests.append(request)
        self._data.sendto(
            build_someip(
                service_id=request.service_id,
                method_id=request.method_id,
                client_id=request.client_id,
                session_id=request.session_id,
                interface_version=request.interface_version,
                message_type=MessageType.RESPONSE,
                payload=request.payload,
            ),
            sender,
        )

    def _run(self) -> None:
        next_offer = time.monotonic()
        try:
            while not self._halt.is_set():
                if self._offering and time.monotonic() >= next_offer:
                    self._send_offer(_OFFER_TTL)
                    next_offer = time.monotonic() + _OFFER_INTERVAL
                try:
                    data, sender = self._data.recvfrom(_RECEIVE_SIZE)
                except socket.timeout:
                    continue
                self._answer(data, sender)
        except Exception as error:  # noqa: BLE001 -- handed to the test thread by _raise_worker_error
            self._worker_error = error
