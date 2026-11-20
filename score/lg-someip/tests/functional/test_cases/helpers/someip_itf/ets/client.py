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

"""SOME/IP client over plain UDP. It uses Scapy to build and parse packets.

The client sends SOME/IP requests directly to a fixed server endpoint.
A background reader thread receives the responses.
It gives the results to the asyncio event loop with ``call_soon_threadsafe``.

Scapy is only a **protocol library** (packet encoding and decoding).
The network I/O uses a normal ``AF_INET / SOCK_DGRAM`` socket.
Thus the client does not need more privileges (``CAP_NET_RAW``).

The SOME/IP test suites use this client as a direct peer of the ETS (Enhanced Testability Service) on the DUT.

Usage::

    client = ScapySomeipClient(
        service_id=0x0103,
        instance_id=0x0091,
        client_ip="160.48.199.1",
        client_port=60010,
        major_version=2,
    )
    await client.discover_endpoint(timeout=15.0)
    client.start(asyncio.get_running_loop())
    response = await client.call_method(38, b"")
    client.close()
"""

from __future__ import annotations

import asyncio
import enum
import socket
import struct
import threading
from dataclasses import dataclass
from typing import Callable

from scapy.contrib.automotive.someip import (
    SD,
    SDENTRY_TYPE_EVTGRP_SUBSCRIBE,
    SDENTRY_TYPE_EVTGRP_SUBSCRIBE_ACK,  # noqa: F401 -- re-exported to the ETS suites
    SDENTRY_TYPE_SRV_FINDSERVICE,
    SDENTRY_TYPE_SRV_OFFERSERVICE,
    SOMEIP,
    SDEntry_EventGroup,
    SDEntry_Service,
    SDOption_IP4_EndPoint,
    SDOption_IP4_Multicast,
)
from scapy.packet import Raw

# This module exports MessageType and ReturnCode again, because the ETS suites import them from here.
from someip_itf.codec import (  # noqa: F401
    L4Proto,
    MessageType,
    ReturnCode,
    SdEntry,
    SdEntryType,
    SomeipResponse,
    parse_sd_datagram,
)

# The SOME/IP header of a non-TP message is always 16 bytes.
_SOMEIP_HEADER_SIZE = 16


class DeliveryPath(enum.Enum):
    """The socket on which the client received a notification."""

    UNICAST = "unicast"
    GROUP = "group"


@dataclass(frozen=True)
class ReceivedNotification:
    """One NOTIFICATION frame that the client received."""

    method_id: int
    payload: bytes
    path: DeliveryPath
    sender: tuple[str, int]


def _sd_entries(data: bytes) -> tuple[SdEntry, ...]:
    """Return the entries of *data* if it is a valid SOME/IP-SD message. If not, return an empty tuple."""
    parsed = parse_sd_datagram(data)
    return parsed[1].entries if parsed is not None else ()


class ScapySomeipClient:
    """SOME/IP client that sends requests over plain UDP to a fixed server endpoint.

    :meth:`discover_endpoint` finds the server endpoint with a SOME/IP-SD Find/Offer exchange.
    Call it before :meth:`start`.
    Tests that do not use SD can give the server IP and port directly with ``server_ip`` / ``server_port``.

    Public interface:

    - :meth:`call_method` — sends a REQUEST, returns the matching RESPONSE.
    - :meth:`subscribe_eventgroup` / :meth:`unsubscribe_eventgroup` — sends a
      SOME/IP-SD ``SubscribeEventgroup`` / ``StopSubscribeEventgroup`` message to
      the SD multicast group. The server then starts or stops sending field notifier
      events to the client endpoint.
    - :meth:`register_callback` — sets a handler for NOTIFICATION frames.
    - :meth:`is_available` — sends a simple method call to the server.

    Args:
        service_id:      SOME/IP service identifier (e.g. ``0x0103``).
        client_ip:       Local bind address. It is the SD endpoint option
                         address and the multicast output interface
                         (e.g. ``"160.48.199.1"``).
        client_port:     Local bind UDP port. The SD endpoint option also
                         contains it (e.g. ``60010``).
        major_version:   Service major version in the ``interface_version``
                         SOME/IP header field. It must be equal to the service deployment
                         (e.g. ``2`` for the ETS service).
        probe_method_id: Method ID that :meth:`is_available` calls.
                         The default is ``37`` (``InterfaceVersion`` getter).
        instance_id:     SOME/IP service instance identifier (e.g. ``0x0091``).
                         SD ``SubscribeEventgroup`` entries need it.
        sd_address:      SOME/IP-SD multicast group address. The default is the
                         AUTOSAR value ``"224.224.224.245"``.
        sd_port:         SOME/IP-SD UDP port. The default is ``30490``.
        server_ip:       Optional server IPv4 address. If ``None`` (default),
                         call :meth:`discover_endpoint` before :meth:`start`
                         to find the server endpoint.
        server_port:     Optional server UDP port. Same rules as ``server_ip``.
        event_multicast_group: Optional ``(address, port)`` of the group to which the server
                         sends multicast events. If set, :meth:`start` joins it
                         on ``client_ip`` and gives its NOTIFICATION frames to the
                         same callback as the unicast frames.
    """

    # These tables are class attributes on purpose, not instance attributes.
    # SOME/IP-SD replies (SUBSCRIBE_ACK/NACK, OfferService) always go to (client_ip, sd_port).
    # The *data* port of the instance that sent the request has no effect.
    # Two instances can have the same client_ip (for example ets_client and ets_client2, with different client_port).
    # Then each binds its own SD monitor socket to the same (INADDR_ANY, sd_port).
    # The kernel gives a unicast reply to only ONE of these sockets.
    # With shared tables, the instance that receives the reply can complete the future of the OTHER instance.
    _sd_subscribe_pending: dict[int, asyncio.Future] = {}
    _sd_unicast_offer_pending: list[asyncio.Future] = []

    def __init__(
        self,
        service_id: int,
        client_ip: str,
        client_port: int,
        major_version: int = 1,
        probe_method_id: int = 37,
        instance_id: int = 0,
        sd_address: str = "224.224.224.245",
        sd_port: int = 30490,
        server_ip: str | None = None,
        server_port: int | None = None,
        event_multicast_group: tuple[str, int] | None = None,
    ) -> None:
        self._service_id = service_id
        self._event_multicast_group = event_multicast_group
        self._instance_id = instance_id
        self._server: tuple[str | None, int | None] = (server_ip, server_port)
        self._major_version = major_version
        self._probe_method_id = probe_method_id
        self._client_ip = client_ip
        self._client_port = client_port
        self._sd_address = sd_address
        self._sd_port = sd_port
        self._session = 1
        self._sd_session = 1
        self._callback: Callable[[int, bytes], None] | None = None
        self._pending: dict[int, asyncio.Future] = {}
        # RESPONSE/ERROR frames with a session_id that is not in _pending.
        # For example, a reply to a different client that arrived here by mistake.
        # Tests use this list as direct proof of an incorrect delivery, not only a timeout of the sender.
        self._orphaned_responses: list[SomeipResponse] = []
        self._loop: asyncio.AbstractEventLoop | None = None
        self._running = False
        self._reader_thread: threading.Thread | None = None
        self._renewal_tasks: dict[int, asyncio.Task] = {}
        # Active subscriptions: eg_id -> (eventgroup, ttl)
        self._active_subscriptions: dict[int, tuple[object, int]] = {}
        self._sd_monitor_sock: socket.socket | None = None
        self._sd_monitor_thread: threading.Thread | None = None
        self._mcast_monitor_sock: socket.socket | None = None
        self._mcast_monitor_thread: threading.Thread | None = None
        # All NOTIFICATIONs received on the two sockets, in arrival order. A test clears it before a burst.
        self.notifications: list[ReceivedNotification] = []
        # OfferService entries for this service instance. The queue gets them only while a test captures them.
        self._offer_entries: asyncio.Queue[SdEntry] | None = None

        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # Bind to INADDR_ANY, so that the socket receives replies on all interfaces.
        # The routing table sets the source IP of sent packets.
        self._sock.bind(("", client_port))
        # Make sure that SD multicast packets go out through the correct bridge interface.
        self._sock.setsockopt(
            socket.IPPROTO_IP,
            socket.IP_MULTICAST_IF,
            socket.inet_aton(client_ip),
        )
        self._sock.settimeout(0.1)

    @property
    def orphaned_responses(self) -> list[SomeipResponse]:
        """RESPONSE and ERROR frames with a session id of no pending call, oldest first."""
        return self._orphaned_responses

    def start(self, loop: asyncio.AbstractEventLoop) -> None:
        """Attach to *loop* and start the background reader thread.

        Call it one time, in a running asyncio event loop, before you call
        :meth:`call_method` or :meth:`is_available`.

        Args:
            loop: The running :class:`asyncio.AbstractEventLoop` to post results into.
        """
        self._loop = loop
        self._running = True
        self._reader_thread = threading.Thread(target=self._read_loop, daemon=True, name="someip-udp-reader")
        self._reader_thread.start()
        # Start the SD monitor. It sends active subscriptions again when the service offers again
        # (for example after suspendInterface, StopOffer, and OfferService).
        sd_mon = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sd_mon.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sd_mon.bind(("", self._sd_port))
        _mreq = struct.pack(
            "4s4s",
            socket.inet_aton(self._sd_address),
            socket.inet_aton(self._client_ip),
        )
        sd_mon.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, _mreq)
        sd_mon.setsockopt(
            socket.IPPROTO_IP,
            socket.IP_MULTICAST_IF,
            socket.inet_aton(self._client_ip),
        )
        # This option tells a direct unicast reply (for example an OfferService answer to a FindService)
        # apart from the periodic multicast Offers of the service. Both arrive on this socket.
        sd_mon.setsockopt(socket.IPPROTO_IP, socket.IP_PKTINFO, 1)
        sd_mon.settimeout(0.1)
        self._sd_monitor_sock = sd_mon
        self._sd_monitor_thread = threading.Thread(target=self._sd_monitor_loop, daemon=True, name="someip-sd-monitor")
        self._sd_monitor_thread.start()
        if self._event_multicast_group is not None:
            self._start_mcast_monitor(*self._event_multicast_group)

    def _start_mcast_monitor(self, group: str, port: int) -> None:
        """Join *group* on ``client_ip`` and start the thread that dispatches its datagrams.

        All clients of one process bind the same port. Thus each client receives its own copy of a group datagram.
        """
        mcast_mon = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        mcast_mon.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        mcast_mon.bind(("", port))
        mreq = struct.pack("4s4s", socket.inet_aton(group), socket.inet_aton(self._client_ip))
        mcast_mon.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        mcast_mon.settimeout(0.1)
        self._mcast_monitor_sock = mcast_mon
        self._mcast_monitor_thread = threading.Thread(
            target=self._mcast_monitor_loop, daemon=True, name="someip-mcast-monitor"
        )
        self._mcast_monitor_thread.start()

    def close(self) -> None:
        """Stop the reader thread and release the UDP socket."""
        self._running = False
        for fut in self._pending.values():
            if not fut.done():
                fut.cancel()
        self._pending.clear()
        self._orphaned_responses.clear()
        for task in self._renewal_tasks.values():
            task.cancel()
        self._renewal_tasks.clear()
        self._sock.close()
        if self._sd_monitor_sock is not None:
            self._sd_monitor_sock.close()
            self._sd_monitor_sock = None
        if self._mcast_monitor_sock is not None:
            self._mcast_monitor_sock.close()
            self._mcast_monitor_sock = None

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    async def call_method(self, method_id: int, payload: bytes) -> SomeipResponse:
        """Send a SOME/IP REQUEST and wait for the related RESPONSE.

        Args:
            method_id: The SOME/IP method identifier.
            payload:   Raw payload bytes; serialisation is the caller's responsibility.

        Returns:
            The parsed :class:`SomeipResponse` with the session ID of this request.
        """
        session_id = self._next_session()
        fut: asyncio.Future = self._loop.create_future()
        self._pending[session_id] = fut
        self._send(method_id, payload, MessageType.REQUEST, session_id)
        try:
            return await fut
        finally:
            # Always remove the pending entry. Cancelled or timed-out futures must not stay in the dict.
            self._pending.pop(session_id, None)

    def send_no_return(self, method_id: int, payload: bytes) -> None:
        """Send a SOME/IP REQUEST_NO_RETURN (fire-and-forget). The client does not wait for a RESPONSE.

        Args:
            method_id: The SOME/IP method identifier.
            payload:   Raw payload bytes; serialisation is the caller's responsibility.
        """
        self._send(method_id, payload, MessageType.REQUEST_NO_RETURN, self._next_session())

    def subscribe_eventgroup(self, eventgroup: object, ttl: int) -> None:
        """Send a SOME/IP-SD ``SubscribeEventgroup`` message to the multicast group.

        The message tells the server to send NOTIFICATION frames of the eventgroup to this client.
        The SD endpoint option has the client endpoint (IP and UDP port), so that the server knows where to send events.
        A background task sends the subscribe again at 80 % of the TTL.
        Thus the subscription does not expire, also in a long test.

        Args:
            eventgroup: An object with an ``.id`` attribute, or a value that
                        converts to an integer. It identifies the eventgroup.
            ttl:        Subscription lifetime in seconds. The renewal interval
                        is ``ttl * 0.8``.
        """
        eg_id = eventgroup.id if hasattr(eventgroup, "id") else int(eventgroup)
        old = self._renewal_tasks.pop(eg_id, None)
        if old is not None:
            old.cancel()
        self._active_subscriptions[eg_id] = (eventgroup, ttl)
        self._send_sd_eventgroup(eventgroup, ttl)
        if ttl > 0 and self._loop is not None:
            interval = max(1.0, ttl * 0.8)

            async def _renew(eg=eventgroup, t=ttl, iv=interval):
                while True:
                    await asyncio.sleep(iv)
                    self._send_sd_eventgroup(eg, t)

            self._renewal_tasks[eg_id] = self._loop.create_task(_renew())

    def unsubscribe_eventgroup(self, eventgroup: object) -> None:
        """Send a SOME/IP-SD ``StopSubscribeEventgroup`` (TTL=0) message.

        The message tells the server to stop the NOTIFICATION frames of the eventgroup to this client endpoint.

        Args:
            eventgroup: Same type as for :meth:`subscribe_eventgroup`.
        """
        self.stop_maintaining_subscription(eventgroup)
        self._send_sd_eventgroup(eventgroup, 0)

    def stop_maintaining_subscription(self, eventgroup: object) -> None:
        """Stop sending the subscription for *eventgroup* again, but do not unsubscribe.

        The client sends no ``StopSubscribeEventgroup``. The server keeps the subscription until its TTL expires
        or the server drops it. The client does not renew it or send it again when the service offers again.

        Args:
            eventgroup: Same type as for :meth:`subscribe_eventgroup`.
        """
        eg_id = eventgroup.id if hasattr(eventgroup, "id") else int(eventgroup)
        self._active_subscriptions.pop(eg_id, None)
        task = self._renewal_tasks.pop(eg_id, None)
        if task is not None:
            task.cancel()

    async def wait_for_subscribe_ack(self, eventgroup: object, timeout: float = 2.0) -> int:
        """Wait for the SUBSCRIBE_ACK/NACK reply of the server to a :meth:`subscribe_eventgroup` call.

        The server sends the reply on the SD channel, unicast to the SD port of this client.
        The reply is an ``SDEntry_EventGroup`` of type ``SUBSCRIBE_ACK``.
        There is no different wire message for the negative case. A NACK is the same entry type with ``ttl == 0``.

        Args:
            eventgroup: Same type as for :meth:`subscribe_eventgroup`.
            timeout: Seconds to wait before the error.

        Returns:
            The TTL of the ACK entry. A value that is not zero confirms the
            subscription. ``0`` is a negative acknowledgement.

        Raises:
            TimeoutError: If no matching ACK/NACK entry arrives in *timeout*.
        """
        return (await self.wait_for_subscribe_ack_entry(eventgroup, timeout)).ttl

    async def wait_for_subscribe_ack_entry(self, eventgroup: object, timeout: float = 2.0) -> SdEntry:
        """Same as :meth:`wait_for_subscribe_ack`, but return the full ACK entry with its options.

        All ACKs for the eventgroup complete the wait, also an ACK for the renewal of a different client.
        An ACK has no client endpoint, and the server sends all ACKs to the same SD port.
        """
        eg_id = eventgroup.id if hasattr(eventgroup, "id") else int(eventgroup)
        fut: asyncio.Future = self._loop.create_future()
        self._sd_subscribe_pending[eg_id] = fut
        try:
            return await asyncio.wait_for(fut, timeout=timeout)
        finally:
            self._sd_subscribe_pending.pop(eg_id, None)

    async def subscribe_and_wait_for_ack(
        self, eventgroup: object, ttl: int = 30, timeout: float = 5.0, settle: float = 0.5
    ) -> SdEntry:
        """Subscribe to *eventgroup*, make sure that the server sends an Ack, and return the Ack entry.

        The renewal of a different client can complete the wait first.
        Thus the call also waits *settle* seconds, so that the server can process this Subscribe.
        Raise AssertionError if the server sends a NACK. Raise TimeoutError if no Ack or NACK arrives in time.
        """
        self.subscribe_eventgroup(eventgroup, ttl)
        ack = await self.wait_for_subscribe_ack_entry(eventgroup, timeout=timeout)
        assert ack.ttl != 0, f"the server rejected the Subscribe to eventgroup {ack.eventgroup_id:#06x}"
        await asyncio.sleep(settle)
        return ack

    @staticmethod
    def ack_multicast_group(ack: SdEntry) -> tuple[str, int] | None:
        """Return the (address, port) of the IPv4 Multicast Option of a SubscribeAck, or None."""
        options = (*ack.options_1, *ack.options_2)
        return next(((o.addr, o.port) for o in options if isinstance(o, SDOption_IP4_Multicast)), None)

    def received_counters(self, event_id: int) -> tuple[list[int], list[int]]:
        """Return the first payload byte of each *event_id* notification received over unicast and over the group.

        The lists keep the arrival order and the duplicates.
        Raise AssertionError if a notification did not come from the offered endpoint.
        """
        by_path: dict[DeliveryPath, list[int]] = {path: [] for path in DeliveryPath}
        for notification in self.notifications:
            if notification.method_id == event_id:
                assert notification.sender == (self.server_ip, self.server_port), "an event not from the offer"
                by_path[notification.path].append(notification.payload[0])
        return by_path[DeliveryPath.UNICAST], by_path[DeliveryPath.GROUP]

    async def wait_for_unicast_offer(self, timeout: float = 1.0) -> bool:
        """Wait for a direct (unicast) OfferService reply for the service of this client.

        A direct reply answers a message from this client, for example a FindService entry.
        The server sends it unicast to the SD port of this client.
        The periodic multicast Offers of the main phase arrive on the same monitor socket.
        They do NOT show that the server processed a specific request.
        ``IP_PKTINFO`` tells the two apart. The IP destination of a periodic Offer is the SD multicast group.
        The IP destination of a direct reply is the address of this client.

        Args:
            timeout: Seconds to wait.

        Returns:
            ``True`` if a matching unicast OfferService entry arrived in
            *timeout*, ``False`` if not.
        """
        fut: asyncio.Future = self._loop.create_future()
        self._sd_unicast_offer_pending.append(fut)
        try:
            return await asyncio.wait_for(fut, timeout=timeout)
        except asyncio.TimeoutError:
            return False
        finally:
            if fut in self._sd_unicast_offer_pending:
                self._sd_unicast_offer_pending.remove(fut)

    def start_offer_capture(self) -> None:
        """Queue the OfferService and StopOffer entries of this service instance for :meth:`next_offer_entry`."""
        self._offer_entries = asyncio.Queue()

    def stop_offer_capture(self) -> None:
        """Stop the OfferService entry queue and drop the entries that were not read."""
        self._offer_entries = None

    async def next_offer_entry(self, stop: bool, timeout: float) -> SdEntry:
        """Return the next captured StopOffer (TTL 0) if *stop* is set, else the next Offer. Skip the other kind.

        Args:
            stop: If True, wait for a StopOffer, not an Offer.
            timeout: Seconds to wait in total.

        Raises:
            asyncio.TimeoutError: If no matching entry arrives in *timeout*.
        """
        queue = self._offer_entries
        assert queue is not None, "next_offer_entry needs start_offer_capture() first"
        deadline = self._loop.time() + timeout
        while True:
            entry = await asyncio.wait_for(queue.get(), timeout=max(0.0, deadline - self._loop.time()))
            if (entry.ttl == 0) == stop:
                return entry

    def register_callback(self, fn: Callable[[int, bytes], None] | None) -> None:
        """Set or clear a callback for received NOTIFICATION frames.

        Args:
            fn: Callable. The client calls ``fn(method_id, payload)`` for each received
                NOTIFICATION. ``None`` removes the current callback.
        """
        self._callback = fn

    async def is_available(self) -> bool:
        """Return ``True`` if the server answers a simple probe call.

        The probe calls :attr:`_probe_method_id` (default ``37``, ``InterfaceVersion`` getter).
        Return ``False`` on timeout or socket error.
        """
        try:
            resp = await asyncio.wait_for(self.call_method(self._probe_method_id, b""), timeout=2.0)
            return resp.return_code == ReturnCode.E_OK
        except (asyncio.TimeoutError, OSError):
            return False

    async def discover_endpoint(self, timeout: float = 15.0) -> None:
        """Find the server endpoint with a SOME/IP-SD Find/Offer exchange.

        The function opens a temporary UDP socket on the SD port and joins the SD multicast group.
        It sends SD FindService messages periodically until a matching OfferService reply arrives.
        It keeps the server IP and UDP port from the ``SDOption_IP4_EndPoint`` of the Offer.
        All later :meth:`call_method` calls use them.

        Call it **before** :meth:`start`.

        Args:
            timeout: Maximum seconds to wait for an SD Offer before the error.

        Raises:
            TimeoutError: If no matching SD Offer arrives in *timeout* seconds.
        """
        sd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sd_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sd_sock.bind(("", self._sd_port))
        mreq = struct.pack(
            "4s4s",
            socket.inet_aton(self._sd_address),
            socket.inet_aton(self._client_ip),
        )
        sd_sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        sd_sock.setsockopt(
            socket.IPPROTO_IP,
            socket.IP_MULTICAST_IF,
            socket.inet_aton(self._client_ip),
        )
        sd_sock.setblocking(False)

        find_entry = SDEntry_Service(
            type=SDENTRY_TYPE_SRV_FINDSERVICE,
            srv_id=self._service_id,
            inst_id=self._instance_id,
            major_ver=self._major_version,
            ttl=3,
            minor_ver=0xFFFFFFFF,
        )
        sd_pkt = SD(entry_array=[find_entry])
        hdr = SOMEIP(
            srv_id=0xFFFF,
            sub_id=0x8100,
            client_id=0,
            session_id=self._next_sd_session(),
            proto_ver=0x01,
            iface_ver=0x01,
            msg_type=int(MessageType.NOTIFICATION),
            retcode=int(ReturnCode.E_OK),
        )
        raw_find = bytes(hdr / sd_pkt)

        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout
        try:
            while loop.time() < deadline:
                sd_sock.sendto(raw_find, (self._sd_address, self._sd_port))
                probe_until = min(loop.time() + 1.0, deadline)
                while loop.time() < probe_until:
                    try:
                        remaining = probe_until - loop.time()
                        data = await asyncio.wait_for(loop.sock_recv(sd_sock, 65535), timeout=remaining)
                    except (asyncio.TimeoutError, TimeoutError):
                        break
                    entries = _sd_entries(data)
                    for entry in entries:
                        if (
                            entry.sd_type == SdEntryType.OfferService
                            and entry.service_id == self._service_id
                            and entry.instance_id == self._instance_id
                        ):
                            # A service with a TCP method (for example echoUINT8RELIABLE) offers two
                            # IP4_EndPoint options, one for each transport. This client uses only UDP.
                            # Thus it must select the UDP option, not the first option.
                            for opt in entry.options_1 + entry.options_2:
                                if isinstance(opt, SDOption_IP4_EndPoint) and opt.l4_proto == L4Proto.UDP:
                                    self._server = (opt.addr, opt.port)
                                    return
        finally:
            try:
                sd_sock.setsockopt(socket.IPPROTO_IP, socket.IP_DROP_MEMBERSHIP, mreq)
            except OSError:
                pass
            sd_sock.close()
        raise TimeoutError(
            f"SD Find/Offer for service 0x{self._service_id:04x}/"
            f"0x{self._instance_id:04x} timed out after {timeout:.0f}s"
        )

    # ------------------------------------------------------------------
    # Internals
    # ------------------------------------------------------------------

    def _next_session(self) -> int:
        """Return the next session ID in the range ``[1, 0xFFFF]``. After 0xFFFF, start at 1 again."""
        sid = self._session
        self._session = (self._session % 0xFFFF) + 1
        return sid

    def _next_sd_session(self) -> int:
        """Return the next SD session ID in the range ``[1, 0xFFFF]``. After 0xFFFF, start at 1 again."""
        sid = self._sd_session
        self._sd_session = (self._sd_session % 0xFFFF) + 1
        return sid

    def _send_sd_eventgroup(self, eventgroup: object, ttl: int) -> None:
        """Build a SOME/IP-SD eventgroup entry and send it to the multicast group.

        :meth:`subscribe_eventgroup` (``ttl > 0``) and :meth:`unsubscribe_eventgroup` (``ttl == 0``) use it.
        The Scapy :class:`~scapy.contrib.automotive.someip.SD` layer builds the SD packet.
        The packet goes out on the same UDP socket as the method calls.

        Args:
            eventgroup: Object with an ``.id`` attribute, or a value that converts
                        to an integer. It identifies the eventgroup.
            ttl:        Entry TTL in seconds. ``0`` means StopSubscribeEventgroup.
        """
        eg_id = eventgroup.id if hasattr(eventgroup, "id") else int(eventgroup)
        ep_opt = SDOption_IP4_EndPoint(
            addr=self._client_ip,
            l4_proto=0x11,  # UDP
            port=self._client_port,
        )
        entry = SDEntry_EventGroup(
            type=SDENTRY_TYPE_EVTGRP_SUBSCRIBE,
            srv_id=self._service_id,
            inst_id=self._instance_id,
            major_ver=self._major_version,
            ttl=ttl,
            eventgroup_id=eg_id,
            n_opt_1=1,
            index_1=0,
        )
        sd = SD(entry_array=[entry], option_array=[ep_opt])
        hdr = SOMEIP(
            srv_id=0xFFFF,
            sub_id=0x8100,
            client_id=0,
            session_id=self._next_sd_session(),
            proto_ver=0x01,
            iface_ver=0x01,
            msg_type=int(MessageType.NOTIFICATION),
            retcode=int(ReturnCode.E_OK),
        )
        self._sd_send_socket().sendto(bytes(hdr / sd), (self._sd_address, self._sd_port))

    def _sd_send_socket(self) -> socket.socket:
        """The socket SD leaves from: bound to the SD port once :meth:`start` ran, since a receiver may drop SD from any other source port."""
        return self._sd_monitor_sock if self._sd_monitor_sock is not None else self._sock

    @property
    def server_port(self) -> int | None:
        """UDP port of the found or configured server endpoint."""
        return self._server[1]

    @property
    def server_ip(self) -> str | None:
        """IPv4 address of the found or configured server endpoint."""
        return self._server[0]

    def send_sd_offer(
        self,
        instance_id: int,
        ip: str,
        port: int,
        ttl: int = 0xFFFFFF,
        minor_ver: int = 0,
    ) -> None:
        """Send a SOME/IP-SD ``OfferService`` for this service id to the SD group.

        Routing tests use it to simulate a *remote* ECU that offers the same service id
        (a different instance) on a given UDP endpoint.
        This tests how the DUT identifies instances in its resolution table.

        Args:
            instance_id: Instance id to offer (not the local one).
            ip:          Advertised endpoint IPv4 address (the "remote" address).
            port:        Advertised endpoint UDP port.
            ttl:         Entry TTL in seconds.
            minor_ver:   Service minor version.
        """
        ep_opt = SDOption_IP4_EndPoint(addr=ip, l4_proto=0x11, port=port)  # UDP
        entry = SDEntry_Service(
            type=SDENTRY_TYPE_SRV_OFFERSERVICE,
            srv_id=self._service_id,
            inst_id=instance_id,
            major_ver=self._major_version,
            minor_ver=minor_ver,
            ttl=ttl,
            n_opt_1=1,
            index_1=0,
        )
        sd = SD(entry_array=[entry], option_array=[ep_opt])
        hdr = SOMEIP(
            srv_id=0xFFFF,
            sub_id=0x8100,
            client_id=0,
            session_id=self._next_sd_session(),
            proto_ver=0x01,
            iface_ver=0x01,
            msg_type=int(MessageType.NOTIFICATION),
            retcode=int(ReturnCode.E_OK),
        )
        self._sd_send_socket().sendto(bytes(hdr / sd), (self._sd_address, self._sd_port))

    def _send(
        self,
        method_id: int,
        payload: bytes,
        msg_type: MessageType,
        session_id: int,
    ) -> None:
        """Build a SOME/IP frame with Scapy and send it to the server endpoint.

        :class:`scapy.contrib.automotive.someip.SOMEIP` builds the header and calculates the ``len`` field.
        If ``payload`` is not empty, the ``/`` operator adds a :class:`scapy.packet.Raw` payload layer.

        Args:
            method_id:  SOME/IP method identifier.
            payload:    Raw payload bytes.
            msg_type:   Message type field value.
            session_id: Session identifier that relates the request to the response.
        """
        pkt = SOMEIP(
            srv_id=self._service_id,
            sub_id=method_id,
            client_id=0x0000,
            session_id=session_id,
            proto_ver=0x01,
            iface_ver=self._major_version,
            msg_type=int(msg_type),
            retcode=int(ReturnCode.E_OK),
        )
        if payload:
            pkt = pkt / Raw(payload)
        if self._server[0] is None or self._server[1] is None:
            raise RuntimeError("Server endpoint not resolved — call discover_endpoint() before start()")
        self._sock.sendto(bytes(pkt), self._server)

    def _dispatch(self, session_id: int, raw: bytes) -> None:
        """Give a received frame to the related future or callback.

        The asyncio event loop always calls it (through ``call_soon_threadsafe``).
        Thus only one thread uses :attr:`_pending` and :attr:`_callback`.

        Args:
            session_id: Session ID that the reader got from the SOME/IP header.
            raw:        Complete raw datagram bytes.
        """
        try:
            resp = SomeipResponse(raw)
        except Exception:
            return  # ignore a malformed frame

        if resp.message_type in (MessageType.RESPONSE, MessageType.ERROR):
            fut = self._pending.pop(session_id, None)
            if fut is not None and not fut.done():
                fut.set_result(resp)
            elif fut is None:
                self._orphaned_responses.append(resp)
        elif resp.message_type == MessageType.NOTIFICATION and self._callback is not None:
            self._callback(resp.method_id, resp.payload)

    def _sd_monitor_loop(self) -> None:
        """Background thread: monitor the SD multicast channel for OfferService.

        When the server offers the service again (for example after StopOffer or suspendInterface),
        the thread sends all active subscriptions again. Thus the server knows this client again.
        It also completes pending :meth:`wait_for_unicast_offer` and :meth:`wait_for_subscribe_ack` calls.
        """
        _PKTINFO_SPACE = socket.CMSG_SPACE(12)
        while self._running:
            try:
                data, ancdata, _flags, _addr = self._sd_monitor_sock.recvmsg(65535, _PKTINFO_SPACE)
            except socket.timeout:
                continue
            except OSError:
                break
            dest_addr = None
            for cmsg_level, cmsg_type, cmsg_data in ancdata:
                if cmsg_level == socket.IPPROTO_IP and cmsg_type == socket.IP_PKTINFO:
                    # struct in_pktinfo { int ipi_ifindex; struct in_addr ipi_spec_dst;
                    # struct in_addr ipi_addr; }. ipi_addr (bytes 8:12) is the
                    # original destination address of the datagram.
                    dest_addr = socket.inet_ntoa(cmsg_data[8:12])
            for entry in _sd_entries(data):
                if (
                    entry.sd_type == SdEntryType.OfferService
                    and entry.service_id == self._service_id
                    and entry.instance_id == self._instance_id
                ):
                    offer_entries = self._offer_entries
                    if offer_entries is not None:
                        self._loop.call_soon_threadsafe(offer_entries.put_nowait, entry)
                    if self._active_subscriptions:
                        self._loop.call_soon_threadsafe(self._resubscribe_all)
                    if dest_addr == self._client_ip:
                        self._loop.call_soon_threadsafe(self._dispatch_unicast_offer)
                    break
                if (
                    entry.sd_type == SdEntryType.SubscribeAck
                    and entry.service_id == self._service_id
                    and entry.instance_id == self._instance_id
                ):
                    self._loop.call_soon_threadsafe(self._dispatch_subscribe_ack, entry)

    def _mcast_monitor_loop(self) -> None:
        """Background thread: give each datagram of the event multicast group to :meth:`_dispatch`."""
        while self._running and self._mcast_monitor_sock is not None:
            try:
                data, addr = self._mcast_monitor_sock.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError:
                break
            if len(data) < _SOMEIP_HEADER_SIZE:
                continue
            self._loop.call_soon_threadsafe(self._record_notification, data, DeliveryPath.GROUP, addr)
            self._loop.call_soon_threadsafe(self._dispatch, SOMEIP(data).session_id, data)

    def _record_notification(self, raw: bytes, path: DeliveryPath, sender: tuple[str, int]) -> None:
        """Add *raw* to :attr:`notifications` if it is a NOTIFICATION. Call it only on the event loop thread."""
        try:
            frame = SomeipResponse(raw)
        except Exception:
            return  # ignore a malformed frame, as _dispatch does
        if frame.message_type == MessageType.NOTIFICATION:
            self.notifications.append(ReceivedNotification(frame.method_id, frame.payload, path, sender))

    def _resubscribe_all(self) -> None:
        """Send all active subscriptions again. Call it only on the event loop thread."""
        for eventgroup, ttl in list(self._active_subscriptions.values()):
            self._send_sd_eventgroup(eventgroup, ttl)

    def _dispatch_unicast_offer(self) -> None:
        """Complete the pending :meth:`wait_for_unicast_offer` futures. Call it only on the event loop thread."""
        for fut in self._sd_unicast_offer_pending:
            if not fut.done():
                fut.set_result(True)

    def _dispatch_subscribe_ack(self, entry: SdEntry) -> None:
        """Complete a pending :meth:`wait_for_subscribe_ack_entry` future. Call it only on the event loop thread."""
        fut = self._sd_subscribe_pending.get(entry.eventgroup_id)
        if fut is not None and not fut.done():
            fut.set_result(entry)

    def _read_loop(self) -> None:
        """Background thread: receive UDP datagrams and schedule their dispatch.

        The thread runs until :attr:`_running` is cleared or the socket closes.
        It gives each datagram to :meth:`_dispatch` through ``call_soon_threadsafe``.
        Thus all changes of the asyncio state occur on the event loop thread.
        """
        while self._running:
            try:
                data, addr = self._sock.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError:
                break
            if len(data) < _SOMEIP_HEADER_SIZE:
                continue
            # Decode the header with Scapy and get the session_id that relates the request to the response.
            session_id = SOMEIP(data).session_id
            self._loop.call_soon_threadsafe(self._record_notification, data, DeliveryPath.UNICAST, addr)
            self._loop.call_soon_threadsafe(self._dispatch, session_id, data)
