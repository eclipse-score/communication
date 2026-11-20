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

"""SD and method response timing tests.

Test classes
------------
SOME/IP-SD initial find and repetition timing
    The SOME/IP-SD client find procedure has these steps:
      1. After a random initial delay [InitialDelayMin, InitialDelayMax], send
         the first FindService.
      2. Send again up to RepetitionMax times, at intervals that increase
         exponentially: RepetitionBaseDelay x 2^n  (n = 0, 1, ..., RepetitionMax-1).
      3. Go into the Main Phase and use the periodic OfferService of the server.
    Checks: the Find count, the initial delay and the repetition intervals.

Cyclic OfferService cadence
    Monitors OfferService messages for 12 s, discards burst intervals shorter than
    one second, and checks that each cyclic-phase interval is
    CyclicOfferPeriod ± 50 ms (1.0 ± 0.05 s).

Rediscovery after SD multicast silence
    The tester leaves the SD multicast group for 4.5 s (> Offer-TTL 3 s + one
    cyclic period), so all entries that it has are expired. Then it joins again
    with a new socket and checks that an OfferService arrives in 1.1 s.

SOME/IP method response delay
    Calls echoUINT8 10 times (after one untimed ARP warm-up call). Each RTT must
    be below 20 ms, which catches a stuck service. The median must be
    below 5 ms on docker or 8 ms on the board, which catches a slow service.
    The median RTT is ~0.9 ms on a Docker bridge and ~4 ms on the SA8650.

SD storm through OfferService oscillation
    Sends suspendInterface(start=0, duration=1) in a loop for 30 s at
    0.5 s intervals (faster than the 1 s re-offer cycle). This causes continuous
    StopOffer/Offer oscillation. After the storm, the service must become
    stable again (echoUINT8 = E_OK) in 3 s.

DUT configuration that the tests use:
  InitialDelayMin     = 0.01 s
  InitialDelayMax     = 0.01 s   (deterministic: min == max)
  RepetitionBaseDelay = 0.03 s
  RepetitionMax       = 3
  FindTTL             = 255
  CyclicOfferPeriod   = 1.0 s
  Offer-TTL           = 3.0 s
"""

from __future__ import annotations

import asyncio
import logging
import socket
import statistics

import pytest
from attribute_plugin import add_test_properties
from perf_itf.board import on_board
from scapy.contrib.automotive.someip import (
    SD,
    SDENTRY_TYPE_SRV_FINDSERVICE,
    SOMEIP,
    SDEntry_Service,
    SDOption_IP4_EndPoint,
)
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import (
    ETS_INSTANCE_ID,
    ETS_MAJOR_VERSION,
    ETS_SERVER_TCP_PORT,
    ETS_SERVER_UDP_PORT,
    ETS_SERVICE_ID,
    METHOD_CLIENT_SERVICE_ACTIVATE,
    METHOD_CLIENT_SERVICE_DEACTIVATE,
    METHOD_ECHO_UINT8,
    METHOD_SUSPEND_INTERFACE,
    SD_MULTICAST_GROUP,
    SD_PORT,
    trigger,
)
from someip_itf.ets.sd import ets_offer_seen_within, is_ets_find, is_ets_offer, sd_multicast_socket

logger = logging.getLogger(__name__)

#: Seconds: InitialDelayMin == InitialDelayMax -> deterministic initial delay.
_INITIAL_DELAY: float = 0.01

#: Seconds: RepetitionBaseDelay. The interval to the first repetition. It doubles each time.
_REP_BASE_DELAY: float = 0.03

#: Integer: RepetitionMax. The number of repetitions after the initial Find.
_REP_MAX: int = 3

#: Seconds: permitted deviation from the configured interval.
#: It must cover only the asyncio scheduling jitter. 10 ms is sufficient for Linux
#: (measured jitter < 2 ms).
_T_TOL: float = 0.010

#: Seconds: maximum time to collect all _REP_MAX + 1 FindService messages.
#: The IPC path of the server from clientServiceActivate to the first Find adds ~800 ms.
#: The full repetition phase adds 0.03 + 0.06 + 0.12 = 0.21 s. With 0.2 s margin,
#: the worst case is ~1.3 s. 2.0 s is a sufficient limit.
_COLLECT_TIMEOUT: float = 2.0

#: Seconds: upper limit for liveness check (a) in
#: ``test_initial_find_repetition_timing``. The first Find must arrive in
#: this time after the clientServiceActivate call. It covers the
#: measured ~800 ms container IPC overhead with 400 ms margin. It must be less
#: than _COLLECT_TIMEOUT, so that the check can fail independently of the
#: collection deadline.
_PROXY_START_MAX: float = 1.2

#: Seconds: CyclicOfferPeriod. The server sends OfferService
#: once in each period in the SD Main Phase.
_CYCLIC_OFFER_PERIOD: float = 1.0

#: Seconds: the time to monitor cyclic OfferService messages. 12 s gives
#: ~10-11 cyclic intervals, which is more than the _MIN_CYCLIC_INTERVALS threshold.
_OBSERVE_SECONDS: float = 12.0

#: Minimum number of cyclic-phase intervals between offers that the
#: cadence check needs to be statistically meaningful.
_MIN_CYCLIC_INTERVALS: int = 8

#: Seconds: cadence tolerance around CyclicOfferPeriod (1.0 ± 0.05 s).
_CADENCE_TOL: float = 0.050

#: Seconds: Offer-TTL. A service-directory entry expires
#: when no new OfferService arrives in this time.
_OFFER_TTL: float = 3.0

#: Seconds the tester stays out of the SD multicast group.
#: Longer than Offer-TTL plus one cyclic period, so an entry from the last offer before the tester left is expired.
_BLOCK_DURATION: float = _OFFER_TTL + _CYCLIC_OFFER_PERIOD + 0.5  # 4.5 s

#: Seconds: maximum wait for a new offer after the tester joins again: one
#: full cyclic offer period (1.0 s) plus a small asyncio scheduling margin.
_RECOVERY_TIMEOUT: float = _CYCLIC_OFFER_PERIOD + 0.100  # 1.1 s

#: Integer: number of round-trip samples for statistical confidence.
_SAMPLE_COUNT: int = 10

#: Seconds: the duration of the rapid suspendInterface storm.
_STORM_DURATION: float = 30.0

#: Seconds: interval between consecutive suspendInterface calls.
#: It must be shorter than _STORM_SUSPEND_DURATION (1 s), so that each new call
#: arrives before the previous re-offer cycle ends. Thus the
#: service oscillates during all of the storm.
_STORM_INTERVAL: float = 0.5

#: Integer: ``duration`` argument for suspendInterface. The server
#: waits this number of seconds before it sends OfferService again.
_STORM_SUSPEND_DURATION: int = 1

#: Seconds: maximum time after the final offer for the service to
#: become stable again and accept a method call. The AUTOSAR SOME/IP-SD protocol gives no
#: numeric limit. It describes Main Phase stabilization only qualitatively, through the configurable
#: CYCLIC_OFFER_DELAY. Thus the value is a project-specific buffer: 3 s of
#: stabilization, plus the residual offer/suspend oscillation of the ETS server
#: after a storm. The container logs show that it oscillates for ~2 s after
#: the last suspendInterface of the client before it becomes stable.
_RESTABILISE_TIMEOUT: float = 5.0

#: Seconds: upper limit for each echoUINT8 Python-level RTT, on all targets.
#:
#: The 10 ms REQUEST-RESPONSE-DELAY of the configuration is not a method
#: response delay. It is in SOMEIP-SD-SERVER-SERVICE-INSTANCE-CONFIG
#: and sets how long someipd waits before it sends OfferService in reply
#: to a FindService (SD timing only). REQUEST/RESPONSE method calls have no
#: artificial delay.
#:
#: The largest measured sample is 6.9 ms on the SA8650 (n=100), so a call above 20 ms is a stuck service, not jitter.
_RESP_RTT_STUCK: float = 0.020

#: Seconds: upper limit for the median echoUINT8 RTT on a docker target.
#: Measured on a Docker bridge (n=10): min=0.73 ms, max=1.25 ms, mean=0.91 ms.
_RESP_RTT_MEDIAN_MAX: float = 0.005

#: Seconds: upper limit for the median echoUINT8 RTT on the SA8650. Its link adds ~3 ms to each call.
#: Measured there (n=100 over 10 runs): median 3.9 ms, run medians 3.1-4.5 ms, max 6.9 ms.
_RESP_RTT_MEDIAN_MAX_BOARD: float = 0.008

pytestmark = pytest.mark.asyncio(loop_scope="class")


class TestSDDiscoverPortAndIP:
    """A multicast FindService gets a unicast OfferService with the IP and port of the DUT."""

    @pytest.mark.xfail(
        strict=True,
        reason=(
            "someipd limitation, confirmed on the real DUT: "
            "HandleRemoteFindService never sends a unicast OfferService "
            "reply for any session_id (it is a TODO stub) "
            "-- a multicast FindService gets no unicast reply, "
            "so this test cannot see the unicast OfferService that the spec requires."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_060"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_multicast_findservice_gets_unicast_offer_with_endpoint(self, ets_dut, tester_ip, host_ip):  # noqa: ARG002
        """(partial): a multicast FindService gets a unicast OfferService with the UDP endpoint of the DUT.

        Partial: the test checks only the UDP endpoint option.
        A socket bound to (*tester_ip*, ``SD_PORT``) that does not join the SD multicast group receives only
        packets sent directly to it. Thus each message that it captures is a unicast
        reply, not one of the usual cyclic multicast offers of the DUT.
        """
        find_entry = SDEntry_Service(
            type=SDENTRY_TYPE_SRV_FINDSERVICE,
            srv_id=ETS_SERVICE_ID,
            inst_id=ETS_INSTANCE_ID,
            major_ver=ETS_MAJOR_VERSION,
            ttl=3,
            minor_ver=0xFFFFFFFF,
        )
        find_frame = bytes(
            SOMEIP(
                srv_id=0xFFFF,
                sub_id=0x8100,
                client_id=0,
                session_id=1,
                proto_ver=0x01,
                iface_ver=0x01,
                msg_type=int(MessageType.NOTIFICATION),
                retcode=int(ReturnCode.E_OK),
            )
            / SD(entry_array=[find_entry])
        )

        loop = asyncio.get_running_loop()
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((tester_ip, SD_PORT))
        # Make sure that the multicast FindService goes out through the tester interface,
        # and not through the interface that the routing table picks in the Docker setup.
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(tester_ip))
        sock.setblocking(False)
        sd_layer = None
        try:
            sock.sendto(find_frame, (SD_MULTICAST_GROUP, SD_PORT))
            deadline = loop.time() + 5.0
            while loop.time() < deadline:
                remaining = deadline - loop.time()
                try:
                    data = await asyncio.wait_for(loop.sock_recv(sock, 65535), timeout=remaining)
                except asyncio.TimeoutError:
                    break
                if not is_ets_offer(data):
                    continue
                pkt = SOMEIP(data)
                raw_payload = b"".join(d.load for d in pkt.data if hasattr(d, "load"))
                if not raw_payload:
                    continue
                sd_layer = SD(raw_payload)
                break
        finally:
            sock.close()

        assert sd_layer is not None, "no unicast OfferService received within 5 s"

        # The OfferService can have a second IPv4EndpointOption for the TCP deployment of echoUINT8RELIABLE.
        # The test filters on l4_proto and does not take the first option.
        # Thus the test does not depend on the sequence of the options.
        endpoint_opts = [
            o for o in (sd_layer.option_array or []) if isinstance(o, SDOption_IP4_EndPoint) and o.l4_proto == 0x11
        ]
        assert endpoint_opts, "OfferService has no UDP IPv4EndpointOption"
        opt = endpoint_opts[0]
        assert opt.addr == host_ip, f"endpoint IP must be {host_ip}; got {opt.addr}"
        assert opt.port == ETS_SERVER_UDP_PORT, (
            f"endpoint port must be {ETS_SERVER_UDP_PORT}; got {opt.port} in the unicast OfferService"
        )


class TestSDOfferTcpEndpoint:
    """The OfferService also gives the TCP endpoint of echoUINT8RELIABLE.

    The test monitors a usual cyclic multicast OfferService, and not the unicast FindService reply
    of TestSDDiscoverPortAndIP. That reply path does not operate at this time (see the xfail there),
    and it would hide this check.
    """

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_060", "SOMEIPSRV_OPTIONS_15"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_offer_advertises_tcp_endpoint(self, ets_dut, tester_ip, host_ip):  # noqa: ARG002
        """(variant): a cyclic OfferService has a TCP IPv4EndpointOption.

        Variant: the test uses a cyclic multicast OfferService in place of the unicast reply to a FindService.
        The option has the address of the DUT and the TCP port of echoUINT8RELIABLE.
        """
        loop = asyncio.get_running_loop()
        sd_layer = None
        async with sd_multicast_socket(tester_ip) as (sd_sock, _mreq):
            deadline = loop.time() + _CYCLIC_OFFER_PERIOD + 1.0
            while loop.time() < deadline:
                remaining = deadline - loop.time()
                try:
                    data = await asyncio.wait_for(loop.sock_recv(sd_sock, 65535), timeout=remaining)
                except asyncio.TimeoutError:
                    break
                if not is_ets_offer(data):
                    continue
                pkt = SOMEIP(data)
                raw_payload = b"".join(d.load for d in pkt.data if hasattr(d, "load"))
                if not raw_payload:
                    continue
                sd_layer = SD(raw_payload)
                break

        assert sd_layer is not None, "no cyclic OfferService received within one cyclic period"

        tcp_opts = [
            o for o in (sd_layer.option_array or []) if isinstance(o, SDOption_IP4_EndPoint) and o.l4_proto == 0x06
        ]
        assert tcp_opts, "OfferService has no TCP IPv4EndpointOption for echoUINT8RELIABLE"
        opt = tcp_opts[0]
        assert opt.addr == host_ip, f"TCP endpoint IP must be {host_ip}; got {opt.addr}"
        assert opt.port == ETS_SERVER_TCP_PORT, f"TCP endpoint port must be {ETS_SERVER_TCP_PORT}; got {opt.port}"


class TestSDRepetitions:
    """AUTOSAR SOME/IP-SD FindService repetition timing conformance."""

    # -----------------------------------------------------------------------
    # kStateMachineLoopDelay = 50 ms quantizes all configured delays to ticks.
    # The measured intervals between Finds on the wire are 50 ms / 100 ms / 150 ms,
    # not the 30 ms / 60 ms / 120 ms that AUTOSAR requires. The test checks
    # the specification and is xfail until the defect is fixed.
    # -----------------------------------------------------------------------
    @pytest.mark.xfail(
        reason=(
            "the SD state machine uses a fixed 50 ms "
            "tick (kStateMachineLoopDelay = 50 ms, RunClientServiceStateMachine()). "
            "Each tick decreases the repetition-phase delay counter by kStateMachineLoopDelay, "
            "and the repetition occurs when it is <= 0. Thus each configured delay goes up to the next "
            "50 ms boundary: RepetitionBaseDelay 30 ms occurs at 50 ms, 60 ms at 100 ms, "
            "120 ms at 150 ms. AUTOSAR requires "
            "the exact configured values. Remove xfail when kStateMachineLoopDelay is "
            "smaller or the delay scheduling uses deadlines."
        ),
        strict=True,
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_100"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_initial_find_repetition_timing(self, ets_client, tester_ip):  # noqa: ARG002
        """(partial): the FindService repetitions after clientServiceActivate obey the configuration.

        Partial: the test monitors only the start phase. It does not check that the main phase has no FindService.

        ``clientServiceActivate(start=0)`` makes the embedded proxy of the ETS server
        start its SOME/IP-SD FindService discovery. The
        test monitors the SD multicast socket and records the arrival time
        of each FindService datagram with the service and instance IDs
        of the ETS service. Then the test checks three properties:

        (a) The first FindService arrives in _PROXY_START_MAX (1.2 s) after the
            activate call.
        (b) The intervals between FindService messages obey the exponential back-off:
            RepetitionBaseDelay x 2^n  (n = 0, 1, ..., RepetitionMax - 1).
        (c) The total FindService count is >= _REP_MAX + 1.
        """
        loop = asyncio.get_running_loop()

        find_timestamps: list[float] = []

        async with sd_multicast_socket(tester_ip) as (sd_sock, _mreq):
            # The SD socket opens before clientServiceActivate goes out, so the test captures the first FindService.
            t_activate = loop.time()
            await trigger(ets_client, METHOD_CLIENT_SERVICE_ACTIVATE, bytes([0]))
            try:
                deadline = loop.time() + _COLLECT_TIMEOUT
                while True:
                    remaining = deadline - loop.time()
                    if remaining <= 0:
                        break
                    if len(find_timestamps) > _REP_MAX:
                        break
                    try:
                        data = await asyncio.wait_for(loop.sock_recv(sd_sock, 65535), timeout=remaining)
                    except asyncio.TimeoutError:
                        break
                    if is_ets_find(data):
                        find_timestamps.append(loop.time())
            finally:
                await trigger(ets_client, METHOD_CLIENT_SERVICE_DEACTIVATE, bytes([0]))

        print(f"\nDiagnostics: {len(find_timestamps)} FindService received")
        if find_timestamps:
            print(f"  t_activate offset (ms): {(find_timestamps[0] - t_activate) * 1000:.1f}")
        else:
            print("  no Finds received")
        for i in range(1, len(find_timestamps)):
            interval_ms = (find_timestamps[i] - find_timestamps[i - 1]) * 1000
            expected_ms = _REP_BASE_DELAY * (2 ** (i - 1)) * 1000
            print(f"  interval[{i - 1}]: {interval_ms:.1f} ms  (configured {expected_ms:.0f} ms)")

        assert len(find_timestamps) >= _REP_MAX + 1, (
            f"Expected >= {_REP_MAX + 1} FindService transmissions from server "
            f"(1 initial + RepetitionMax={_REP_MAX} repetitions), "
            f"got {len(find_timestamps)}"
        )

        t_first_from_activate = find_timestamps[0] - t_activate
        assert t_first_from_activate <= _PROXY_START_MAX, (
            f"First server FindService arrived {t_first_from_activate * 1000:.0f} ms "
            f"after clientServiceActivate; expected <= "
            f"{_PROXY_START_MAX * 1000:.0f} ms — "
            "clientServiceActivate may not have triggered SD discovery"
        )

        for rep in range(_REP_MAX):
            expected_interval = _REP_BASE_DELAY * (2**rep)
            actual_interval = find_timestamps[rep + 1] - find_timestamps[rep]
            assert abs(actual_interval - expected_interval) <= _T_TOL, (
                f"Server FindService interval[{rep}]: measured "
                f"{actual_interval * 1000:.1f} ms, "
                f"configured {expected_interval * 1000:.0f} ms "
                f"± {_T_TOL * 1000:.0f} ms "
                f"(RepetitionBaseDelay x 2^{rep})"
            )


class TestSDOfferCadence:
    """AUTOSAR SOME/IP-SD cyclic OfferService cadence conformance."""

    @add_test_properties(
        partially_verifies=["SOMEIPSRV_SD_BEHAVIOR_02"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_cyclic_offer_cadence(self, ets_dut, tester_ip):  # noqa: ARG002
        """The server sends OfferService at the configured cyclic period.

        The test monitors the SD multicast socket for _OBSERVE_SECONDS
        seconds and records the arrival time of each OfferService for the
        ETS service. It calculates all intervals between consecutive offers and discards
        the intervals of the initial rapid-offer burst (< 0.5 s). Then it checks
        that each remaining cyclic-phase interval is in
        _CYCLIC_OFFER_PERIOD ± _CADENCE_TOL.
        """
        loop = asyncio.get_running_loop()

        offer_timestamps: list[float] = []
        deadline = loop.time() + _OBSERVE_SECONDS

        async with sd_multicast_socket(tester_ip) as (sd_sock, _mreq):
            while True:
                remaining = deadline - loop.time()
                if remaining <= 0:
                    break
                try:
                    data = await asyncio.wait_for(
                        loop.sock_recv(sd_sock, 65535),
                        timeout=remaining,
                    )
                except asyncio.TimeoutError:
                    break
                if is_ets_offer(data):
                    offer_timestamps.append(loop.time())

        all_intervals = [offer_timestamps[i + 1] - offer_timestamps[i] for i in range(len(offer_timestamps) - 1)]

        # Discard the short intervals of the initial rapid-offer burst
        # (InitialDelay + RepetitionMax repetitions give gaps < 0.5 s).
        cyclic_intervals = [iv for iv in all_intervals if iv > 0.5]

        assert len(cyclic_intervals) >= _MIN_CYCLIC_INTERVALS, (
            f"Expected >= {_MIN_CYCLIC_INTERVALS} cyclic OfferService intervals "
            f"in {_OBSERVE_SECONDS:.0f} s window, got {len(cyclic_intervals)}. "
            f"All observed intervals (ms): "
            f"{[f'{iv * 1000:.0f}' for iv in all_intervals]}"
        )

        for i, iv in enumerate(cyclic_intervals):
            assert abs(iv - _CYCLIC_OFFER_PERIOD) <= _CADENCE_TOL, (
                f"Cyclic interval #{i + 1}: {iv * 1000:.1f} ms, "
                f"expected {_CYCLIC_OFFER_PERIOD * 1000:.0f} ms "
                f"± {_CADENCE_TOL * 1000:.0f} ms "
                f"(CyclicOfferPeriod ± tolerance)"
            )


class TestSDOfferTtlDrop:
    """Rediscovery after the tester receives no SD multicast for longer than Offer-TTL."""

    async def test_offer_ttl_drop_and_recovery(self, ets_dut, tester_ip):  # noqa: ARG002
        """A listener that joins again after more than Offer-TTL of silence finds the service again.

        The silence is only on the tester side, so the test does not check the Offer-TTL handling of the DUT.
        """
        # Phase 1: Baseline. Make sure that OfferService arrives normally.
        assert await ets_offer_seen_within(tester_ip, 2.5), (
            "No OfferService received in 2.5 s baseline window; ETS server not running or SD multicast not reachable"
        )

        # Phase 2: Silence. The baseline socket left the group when it returned.
        await asyncio.sleep(_BLOCK_DURATION)

        # Phase 3: Recovery. Join again with a new socket. The next offer must come in one cyclic period plus jitter.
        assert await ets_offer_seen_within(tester_ip, _RECOVERY_TIMEOUT), (
            f"No OfferService received within {_RECOVERY_TIMEOUT:.1f} s of rejoining the SD group "
            f"after {_BLOCK_DURATION:.1f} s of silence."
        )


class TestMethodResponseDelay:
    """SOME/IP method response delay."""

    async def test_echo_uint8_response_delay(self, ets_client, request):
        """The echoUINT8 method completes in the SOME/IP method RTT limits.

        The measured echoUINT8 RTT is ~1 ms on a Docker bridge network and ~4 ms on the SA8650. This
        test checks that:
          (a) echoUINT8 returns E_OK with the correct echo payload,
          (b) each measured RTT is below _RESP_RTT_STUCK (20 ms). This
              catches a stuck service or a blocked thread.
          (c) the median RTT is below the limit of the target (5 ms on docker, 8 ms on the board). This
              catches unexpected queuing, but one call with jitter does not cause a failure.

        The test first sends one warm-up call, so that ARP resolution for the
        service port (34501) is complete before the measurement starts.
        """
        loop = asyncio.get_running_loop()

        # Warm-up: ARP resolution for the service port occurs here.
        await asyncio.wait_for(ets_client.call_method(METHOD_ECHO_UINT8, b"\x42"), timeout=2.0)

        rtts: list[float] = []
        for _ in range(_SAMPLE_COUNT):
            t0 = loop.time()
            resp = await asyncio.wait_for(
                ets_client.call_method(METHOD_ECHO_UINT8, b"\x42"),
                timeout=1.0,
            )
            t1 = loop.time()

            assert resp.return_code == ReturnCode.E_OK, f"echoUINT8 returned {resp.return_code!r}; expected E_OK"
            assert resp.payload == b"\x42", f"echoUINT8 echo mismatch: sent 0x42, got {resp.payload.hex()}"
            rtts.append(t1 - t0)

        # The test reports each sample, so a failure shows if one call or all calls went above the limit.
        samples_ms = [f"{r * 1000:.2f}" for r in rtts]
        logger.info("echoUINT8 RTT samples (ms): %s", samples_ms)

        for i, rtt in enumerate(rtts):
            assert rtt <= _RESP_RTT_STUCK, (
                f"Sample {i + 1}: RTT {rtt * 1000:.1f} ms > upper limit "
                f"{_RESP_RTT_STUCK * 1000:.0f} ms — service appears stuck; all samples (ms): {samples_ms}"
            )

        median_max = _RESP_RTT_MEDIAN_MAX_BOARD if on_board(request.config) else _RESP_RTT_MEDIAN_MAX
        median_rtt = statistics.median(rtts)
        assert median_rtt <= median_max, (
            f"Median RTT {median_rtt * 1000:.1f} ms > upper limit "
            f"{median_max * 1000:.0f} ms — responses are queuing; all samples (ms): {samples_ms}"
        )


class TestSDStorm:
    """The service binding continues to operate after rapid SD OfferService oscillation."""

    async def test_sd_storm_via_suspend_oscillation(self, ets_client):
        """The SOME/IP binding continues to operate after 30 s of rapid suspendInterface calls.

        The test sends ``suspendInterface(start=0, duration=1)`` each _STORM_INTERVAL
        (0.5 s) for _STORM_DURATION (30 s). The storm interval is
        shorter than the re-offer cycle of the server (1 s), so the service is not
        stable during the storm. StopOffer and Offer SD messages alternate
        at high frequency. This stresses the SD state machine and the availability
        records of the binding.

        After the storm, the test tries ``echoUINT8(0x42)`` again in 0.4 s
        attempts (with a 0.1 s wait between them) for up to _RESTABILISE_TIMEOUT (5 s).
        """
        loop = asyncio.get_running_loop()

        assert await ets_client.is_available(), "pre-condition: ETS service not available before storm"

        # Storm phase: send suspendInterface(0, 1) each _STORM_INTERVAL
        # seconds for _STORM_DURATION seconds.
        storm_count = 0
        storm_deadline = loop.time() + _STORM_DURATION
        while loop.time() < storm_deadline:
            await trigger(ets_client, METHOD_SUSPEND_INTERFACE, bytes([0, _STORM_SUSPEND_DURATION]))
            await asyncio.sleep(_STORM_INTERVAL)
            storm_count += 1

        min_expected = int(_STORM_DURATION / _STORM_INTERVAL) // 2
        assert storm_count >= min_expected, (
            f"Storm gave only {storm_count} iterations "
            f"(expected >= {min_expected}); asyncio scheduler may be "
            "overloaded"
        )

        # Recovery phase: try echoUINT8 again in 0.4 s attempts during the
        # _RESTABILISE_TIMEOUT time.
        t_recovery_start = loop.time()
        recovered = False
        while loop.time() - t_recovery_start < _RESTABILISE_TIMEOUT:
            try:
                resp = await asyncio.wait_for(
                    ets_client.call_method(METHOD_ECHO_UINT8, b"\x42"),
                    timeout=0.4,
                )
                if resp.return_code == ReturnCode.E_OK and resp.payload == b"\x42":
                    recovered = True
                    break
            except (asyncio.TimeoutError, Exception):  # noqa: BLE001
                pass  # endpoint not ready yet, try again
            await asyncio.sleep(0.1)

        assert recovered, (
            f"Service did not re-stabilise within {_RESTABILISE_TIMEOUT:.0f} s "
            "after the SD storm ended. echoUINT8 never returned E_OK; "
            "method binding is possibly stuck."
        )
