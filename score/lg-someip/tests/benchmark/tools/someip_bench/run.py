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

"""One SOME/IP send-path benchmark run.

someip-daemon, the sender and tcpdump run on the target. someipy runs on the host.
The target is a QNX qemu or a QNX board. SSH controls the two.
Each run uploads the binaries and the configuration. The capture runs on the target.
Thus T_wire and T_app are in one clock domain.
A latency run captures and does not sample. A resource run samples CPU and memory and does not capture.
"""

from __future__ import annotations

import contextlib
import logging
import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Final, Iterator

from perf_itf.artifacts import outputs_dir, pull_file
from perf_itf.process import kill_by_name
from perf_itf.sampling import (
    analyze_cpu_log,
    analyze_mem_log,
    disable_tso,
    enable_tso,
    resolve_pids,
    start_capture,
    start_cpu_sampler,
    stop_capture,
    stop_cpu_sampler,
)
from perf_itf.shell import sh, text
from perf_itf.waiting import wait_until

from someip_bench.paths import BenchPaths
from someip_bench.subscriber import (
    SD_GROUP,
    SD_PORT,
    SUBSCRIBED_TIMEOUT_S,
    client_ports,
    read_summary,
    start_subscriber,
)

_logger = logging.getLogger("someip_benchmark")

DAEMON: Final = "someip-daemon"
SENDER_APP: Final = "bench_sender"
RPC_CLIENT_APP: Final = "rpc_client"

# Placeholder unicast address in the built configurations. lg-someip starts only on an address of a local interface.
_BUILD_IP: Final = "160.48.201.101"

# The QNX channel name that the daemon attaches when it accepts applications. The kernel removes it at daemon exit.
_DAEMON_CHANNEL: Final = "/dev/name/local/someip/someip-0000"

# The file name of the uploaded configuration in the deploy root.
_CONFIG: Final = "bench_config.json"

# Sender run settings.
# These values set the offered load. They are part of the comparison between runs. Do not change them for one run.
# SEND_GAP_US adds a gap after each send. Thus the sender is not faster than the forward path of someipd.
# Without the gap, the full event matrix shows large, incorrect loss.
# QNX rounds the 1000 us sleep up to the next 1 ms clock tick. Thus one send takes 2 ms and the offered load is 500
# events/s.
# The iteration count is the sender default of 1000 events per type.
# Three repetitions give 3000 samples to each bucket. Thus its p99 comes from the 30 slowest sends.
# The measured phase of a latency run takes approximately 120 s.
# A resource run needs the same load but no tail statistics. Thus one repetition is sufficient.
_SUBSCRIBE_WAIT_MS: Final = 8000
_REPETITIONS: Final = 3
_RESOURCE_REPETITIONS: Final = 1
_SEND_GAP_US: Final = 1000
_SEND_RETRY_MS: Final = 50

# After its start, the sender completes in much less time. The limit stops a run that hangs.
_SENDER_TIMEOUT_S: Final = 900

# CPU and memory sampling per process for a resource run.
_CPU_INTERVAL_S: Final = 0.5
_CPU_TARGETS: Final = (DAEMON, SENDER_APP)

# Target-side processes of a run. A kill by name occurs before each run, thus a previous failed run has no effect.
# The list has the two apps, because the previous run on the device can be the other app.
# The list does not have the CPU sampler, because a kill by name is not possible. On the device it is a shell loop
# named "sh". A kill of its "pidin" or "grep" child only makes the loop start a new child. Thus the pidfile stops it.
_TARGET_PROCESSES: Final = ("tcpdump", DAEMON, SENDER_APP, RPC_CLIENT_APP)

# The time for the someipd endpoints to start after someipd accepts applications.
_SOMEIPD_SETTLE_S: Final = 5

_LOG_TAIL_LINES: Final = 40


def _pin(cpu: int | None) -> str:
    """Return the QNX ``on -C`` prefix that pins the next binary to *cpu*. ``on`` execs it, thus the pid stays."""
    return "" if cpu is None else f"on -C {cpu} "


@dataclass(frozen=True)
class Topology:
    """The locations of the parts of this run."""

    root: str  # deploy root on the target: the scratch root, because a RAM /tmp has no directories
    paths: BenchPaths  # target-side scratch paths for this run
    someipd_ip: str  # address that the daemon binds. It replaces the placeholder in the uploaded configuration
    capture_iface: str  # target NIC that sends the notifications (the T_wire capture point)
    subscriber_ip: str  # host address that the someipy peer binds
    someipd_cpu: int | None = None  # target CPU of the daemon. None: no pinning
    app_cpu: int | None = None  # target CPU of the app. None: no pinning
    capture_cpu: int | None = None  # target CPU of tcpdump. None: no pinning

    @classmethod
    def from_config(cls, config, target) -> Topology:
        """Return the topology from the suite options, with the ITF address of *target*."""
        subscriber_ip = config.getoption("--host-subscriber-ip")
        assert subscriber_ip, "--host-subscriber-ip is required"
        paths = BenchPaths.for_root(config.getoption("--target-tmp-dir"))
        return cls(
            root=paths.root,
            paths=paths,
            someipd_ip=target.get_ip(),
            capture_iface=config.getoption("--capture-iface"),
            subscriber_ip=subscriber_ip,
            someipd_cpu=config.getoption("--someipd-cpu"),
            app_cpu=config.getoption("--app-cpu"),
            capture_cpu=config.getoption("--capture-cpu"),
        )


@dataclass(frozen=True)
class Deploy:
    """Host paths of the files that one run uploads: the routing daemon, the app and its lg-someip configuration."""

    daemon: Path
    app: Path
    config: Path

    @classmethod
    def from_config(cls, config) -> Deploy:
        """Return the three files from the ``--bench-*`` options of the target."""
        paths = {}
        for name in ("daemon", "app", "config"):
            value = config.getoption(f"--bench-{name}")
            assert value, f"--bench-{name} is required"
            paths[name] = Path(value).resolve()
        return cls(**paths)


@dataclass
class BenchmarkResult:
    """The artifacts of one run, on the host.

    A latency run sets ``pcap_path`` and ``sendlog_path``, which the analyzer pairs. In a resource run, they are None.
    A resource run sets ``cpu_stats`` and ``mem_stats``. In a latency run, they are empty.
    ``client_ports`` maps the endpoint port of each subscriber to its label.
    The analyzer needs it to assign samples and losses to each subscriber.
    ``subscriber_summary`` is empty if the subscriber wrote no summary.
    """

    transport: str
    num_clients: int
    pcap_path: str | None
    sendlog_path: str | None
    subscriber_summary: dict
    cpu_stats: list = field(default_factory=list)
    mem_stats: list = field(default_factory=list)
    client_ports: dict = field(default_factory=dict)


@dataclass
class BoardSession:
    """The state of one run for its teardown: what to stop, and what the run stopped itself."""

    socket_path: str  # the someipyd socket of the host peer
    procs: list = field(default_factory=list)  # target-side process handles, stopped in reverse order
    host: HostProcess | None = None  # the host-side peer, while it runs
    stopped: bool = False  # the run stopped the capture and the sampler itself
    tso_iface: str | None = None  # the NIC with TSO off. The teardown sets TSO on again


class SomeipBoardRun:
    """Shared parts of all SOME/IP benchmark runs on a target: deploy, daemon, capture, teardown and logs.

    A subclass adds its app, its host-side peer and their start order in its ``run()``.
    """

    def __init__(self, target, topology: Topology, *, app: str, transport: str, tmp_path: Path) -> None:
        """*app* is the name of the target-side app. It is also its file name in the deploy root."""
        self._target = target
        self._topology = topology
        self._paths = topology.paths
        self._app = app
        self._transport = transport
        self._tmp_path = tmp_path

    def deploy(self, deploy: Deploy) -> None:
        """Upload the daemon, the app and the configuration. Replace the placeholder address with the target address.

        The target is not reset between runs. Thus first kill and delete what a previous run left.
        A daemon that continues to run holds the SD port and its local sockets.
        A tcpdump that continues to run adds data to the pcap.
        The host changes the configuration. QNX sed puts its ``-i`` temporary file in /tmp.
        The boot image links /tmp to /dev/shmem, and that filesystem does not support rename().
        """
        self._clean_device()

        root = self._topology.root
        self._sh(f"mkdir -p {root}")
        text_config = deploy.config.read_text()
        assert _BUILD_IP in text_config, f"{deploy.config} does not carry {_BUILD_IP}"
        patched = self._tmp_path / _CONFIG
        patched.write_text(text_config.replace(_BUILD_IP, self._topology.someipd_ip))
        for local, name in ((deploy.daemon, DAEMON), (deploy.app, self._app), (patched, _CONFIG)):
            self._target.upload(str(local), f"{root}/{name}")
        self._sh(f"chmod +x {root}/{DAEMON} {root}/{self._app}")

    @contextlib.contextmanager
    def _board_session(self, host_log: str, *, capture: bool = True) -> Iterator[BoardSession]:
        """Start the capture and the daemon, yield the session for the steps of the run, then stop all.

        *host_log* is the log of the host-side peer. It goes directly to the outputs directory.
        Thus a failed run keeps it without a copy step.
        Without *capture*, tcpdump does not run. Thus it uses no CPU of the processes that a resource run samples.
        The body adds its target processes to ``procs`` and keeps its host peer in ``host``.
        It sets ``stopped`` after it stops the capture and the sampler. This function stops all other parts.
        """
        paths = self._paths
        # The --target-tmp-dir directory possibly does not exist in the image.
        self._sh(f"mkdir -p {paths.root}")
        # Unique for each pytest process. The host /tmp continues to exist after a run.
        # Thus the next run can connect to an old socket of a killed run.
        # Directly in /tmp, not in tmp_path. An AF_UNIX path has a maximum of 108 bytes.
        # A deep TMPDIR (approximately 190 in a bazel sandbox) is too long. This shows only as a host peer timeout.
        session = BoardSession(socket_path=f"/tmp/someipyd-{os.getpid()}.sock")
        failed = True
        try:
            if capture:
                # With TSO, a TCP message goes out as one frame. Without TSO, it goes out as segments.
                # UDP-TP always sends segments. Thus a TCP capture runs without TSO, to stamp the two transports
                # at the same point.
                iface = self._topology.capture_iface
                if self._transport == "tcp" and disable_tso(self._target, iface=iface):
                    session.tso_iface = iface
                    self._sh(f"touch {paths.tso_marker}")
                session.procs.append(
                    start_capture(
                        self._target,
                        qnx=True,
                        iface=iface,
                        bpf=self._transport,
                        pcap=paths.pcap,
                        log=paths.capture_log,
                        cpu=self._topology.capture_cpu,
                    )
                )
            # A channel name that exists is from a daemon that runs. A new daemon cannot attach,
            # and the old daemon passes the ready check.
            if self._daemon_ready():
                raise RuntimeError(f"a {DAEMON} is still running: {_DAEMON_CHANNEL} exists")
            session.procs.append(self._launch_someipd())
            self._wait_for_someipd()
            yield session
            failed = False
        finally:
            # Stop the host peer before you read its log. It keeps the file open for all of its life.
            # After a failure, it continues to write the lines that explain the failure.
            if session.host is not None:
                session.host.stop()
            Path(session.socket_path).unlink(missing_ok=True)
            # A run that raised an error did not do its stops. On the device, nothing else does them.
            if not session.stopped:
                self._stop_target_samplers()
            if session.tso_iface is not None:
                with contextlib.suppress(Exception):
                    enable_tso(self._target, iface=session.tso_iface)
                    self._sh(f"rm -f {paths.tso_marker}", check=False)
            # Stop each target-side process before you copy its log. someipd and the app keep their log open
            # for all of their life. After a failure, they continue to write the lines that explain it.
            # Each command uses exec. Thus these handles have the real pids.
            for proc in reversed(session.procs):
                with contextlib.suppress(Exception):
                    proc.stop()
            self._collect_run_logs(failed=failed)
            if failed:
                _log_tail(host_log, host_log)  # the log of the host-side peer, already on the host

    def _sh(self, command: str, *, check: bool = True) -> str:
        # Without stderr. The callers parse the output, and a warning on stderr can corrupt it.
        return sh(self._target, command, check=check, merge_stderr=False)

    def _launch(self, command: str):
        """Start *command* in the deploy root and return its ITF async handle."""
        return self._target.execute_async(command, cwd=self._topology.root)

    def _launch_app(self, env: str):
        """Start the app with the uploaded configuration and *env*. Write its output to the sender log of the run."""
        cpu = self._topology.app_cpu
        _logger.info("%s on CPU %s", self._app, cpu if cpu is not None else "any")
        return self._launch(f"{env} exec {_pin(cpu)}./{self._app} {_CONFIG} > {self._paths.sender_log} 2>&1")

    def _await_app(self, proc, timeout: float, what: str) -> None:
        """Wait until the app of *proc* exits, then make sure that its log contains ``BENCHMARK DONE``.

        The wait monitors the SSH channel of the start. Thus it runs no command on the target during the measurement.
        An app that exits without the marker failed. The error contains the end of its log.
        """
        try:
            proc.wait(timeout_s=timeout)
        except RuntimeError as exc:
            raise RuntimeError(
                f"{what} still running after {timeout}s\n{self._log_tail(self._paths.sender_log)}"
            ) from exc
        if self._target.execute(f"grep -q 'BENCHMARK DONE' {self._paths.sender_log}")[0] != 0:
            raise RuntimeError(f"{what} exited without BENCHMARK DONE\n{self._log_tail(self._paths.sender_log)}")

    def _launch_someipd(self):
        """Start the routing daemon with the uploaded configuration."""
        cpu = self._topology.someipd_cpu
        _logger.info("%s on CPU %s", DAEMON, cpu if cpu is not None else "any")
        return self._launch(f"exec {_pin(cpu)}./{DAEMON} {_CONFIG} > {self._paths.someipd_log} 2>&1")

    def _daemon_ready(self) -> bool:
        """True if a daemon accepts applications now."""
        return self._target.execute(f"test -e {_DAEMON_CHANNEL}")[0] == 0

    def _wait_for_someipd(self, timeout: float = 30) -> None:
        """Wait until the daemon accepts applications. Then the app can register with a routing daemon.

        A daemon that does not accept applications, or that then fails its network bind, has an incorrect configuration.
        This function reports the two cases. Without it, they show as a discovery timeout on one side.
        """
        if not wait_until(self._daemon_ready, timeout=timeout, interval=0.3):
            raise RuntimeError(
                f"{DAEMON} did not create {_DAEMON_CHANNEL} within {timeout}s\n"
                f"{self._log_tail(self._paths.someipd_log)}"
            )

        # The daemon accepts applications before its network endpoints start. Thus wait a fixed time for them.
        # Its configuration logs at WARNING. At this level, each Socket::bind line is a failure.
        time.sleep(_SOMEIPD_SETTLE_S)

        failures = text(
            self._target.execute(f"grep -n 'Socket::bind' {self._paths.someipd_log} 2>/dev/null")[1]
        ).strip()
        if failures:
            raise RuntimeError(f"{DAEMON} failed to bind an endpoint -- check the rewritten IP\n{failures}")

    def _log_tail(self, log_path: str) -> str:
        """Return the last lines of a target-side log, or an empty string if the log does not exist."""
        return text(self._target.execute(f"tail -n {_LOG_TAIL_LINES} {log_path} 2>/dev/null")[1])

    def _clean_device(self) -> None:
        """Kill the benchmark processes on the device. Delete the files of a previous run from the scratch root.

        Stop the CPU sampler first, with the pidfile. The pidfile is the only handle to the detached ``sh`` loop.
        If you delete the pidfile first, the loop continues to start processes in all later measurements.
        """
        stop_cpu_sampler(self._target, qnx=True, pidfile=self._paths.cpu_pidfile)
        kill_by_name(self._target, qnx=True, names=_TARGET_PROCESSES, signal_name="KILL")
        # A TCP run that was killed before its teardown leaves TSO off. Only its marker shows this.
        if self._target.execute(f"test -f {self._paths.tso_marker}")[0] == 0:
            enable_tso(self._target, iface=self._topology.capture_iface)
            self._sh(f"rm -f {self._paths.tso_marker}", check=False)
        time.sleep(0.5)
        self._sh(f"rm -f {' '.join(self._paths.stale_artifacts)} 2>/dev/null; true", check=False)

    def _stop_target_samplers(self) -> None:
        """Stop the CPU sampler and the capture. Report failures only as warnings.

        They do not stop with the SSH sessions of the run. This function runs in the teardown.
        There, an error must not replace the error that is under diagnosis.
        """
        for what, stop in (
            ("CPU sampler", lambda: stop_cpu_sampler(self._target, qnx=True, pidfile=self._paths.cpu_pidfile)),
            (
                "capture",
                lambda: stop_capture(self._target, qnx=True, log=self._paths.capture_log, wait_for_flush=False),
            ),
        ):
            try:
                stop()
            except Exception:  # noqa: BLE001
                _logger.warning("teardown: stopping the target-side %s failed", what, exc_info=True)

    def _collect_run_logs(self, *, failed: bool) -> None:
        """Copy the target-side process logs into the test outputs. After a failure, log the end of each log.

        The clean-up of the next run deletes them. Thus copy them after each run.
        Each step ignores errors. This function runs after a failure, and a diagnostic error must not replace
        the error that is under diagnosis.
        """
        dest = outputs_dir(self._tmp_path)
        for remote in self._paths.run_logs:
            local = dest / Path(remote).name
            try:
                pulled = pull_file(self._target, remote, str(local))
            except Exception:  # noqa: BLE001
                continue
            if pulled and failed:
                _log_tail(local, remote)


class SomeipBenchmark(SomeipBoardRun):
    """The send-path benchmark on one board. It uploads the deploy, runs the stack one time and copies all artifacts.

    The start order is mandatory. The capture must start before the first frame.
    someipd must route before the sender registers.
    The subscriber must print SUBSCRIBED in the grace period of the sender.
    The teardown uses the reverse order. It stops the subscriber, the sampler and the capture before it copies files.
    """

    def __init__(
        self,
        target,
        topology: Topology,
        *,
        transport: str,
        num_clients: int,
        tmp_path: Path,
        subscriber: Path,
        resources: bool = False,
    ) -> None:
        """*subscriber* is the host-side subscriber script. *resources* selects a resource run."""
        super().__init__(target, topology, app=SENDER_APP, transport=transport, tmp_path=tmp_path)
        self._num_clients = num_clients
        self._subscriber = subscriber
        self._resources = resources

    def run(self) -> BenchmarkResult:
        """Run someipd and the sender on the target and someipy on the host. Collect the artifacts.

        A latency run adds tcpdump on the target. A resource run adds the CPU and memory sampler.
        """
        target, topology, paths = self._target, self._topology, self._paths
        sub_log = str(outputs_dir(self._tmp_path) / "subscriber.log")
        sub_summary = str(self._tmp_path / "sub_summary.json")
        with self._board_session(sub_log, capture=not self._resources) as session:
            sender = self._launch_sender()
            session.procs.append(sender)
            sender_started = time.monotonic()
            if self._resources:
                # Find the pids after the sender exec, because the memory sampler reads them.
                procs = resolve_pids(target, qnx=True, names=_CPU_TARGETS)
                missing = set(_CPU_TARGETS) - {name for name, _pid in procs}
                if missing:
                    raise RuntimeError(f"no pid for {sorted(missing)}, so the run would sample without them")
                start_cpu_sampler(
                    target,
                    qnx=True,
                    procs=procs,
                    log=paths.cpu_log,
                    pidfile=paths.cpu_pidfile,
                    interval_s=_CPU_INTERVAL_S,
                    mem_log=paths.mem_log,
                )

            session.host = start_subscriber(
                self._subscriber,
                subscriber_ip=topology.subscriber_ip,
                transport=self._transport,
                num_clients=self._num_clients,
                summary_path=sub_summary,
                log_path=sub_log,
                socket_path=session.socket_path,
                unicast_sd=False,
            )
            try:
                session.host.wait_for_marker("SUBSCRIBED", SUBSCRIBED_TIMEOUT_S)
            except RuntimeError as exc:
                # The subscriber can only report that it got no offer. Only the target capture shows the cause.
                if self._resources:
                    raise  # no capture, thus no SD traffic to examine
                raise RuntimeError(f"{exc}\n\n{self._diagnose_sd()}") from exc
            # The grace period of the sender is only a timer, not a handshake. If the subscription completes after it,
            # the first measured sends have no receiver. This makes the table incorrect but does not cause a failure.
            subscribed_after_ms = (time.monotonic() - sender_started) * 1000
            if subscribed_after_ms > _SUBSCRIBE_WAIT_MS:
                _logger.error(
                    "subscription completed %.0f ms after the sender started, past its %d ms grace period: the "
                    "first measured events were emitted unsubscribed and are missing from the paired samples",
                    subscribed_after_ms,
                    _SUBSCRIBE_WAIT_MS,
                )

            # The subscriber writes its summary when it stops. tcpdump writes the pcap when it stops.
            self._await_app(sender, _SENDER_TIMEOUT_S, "sender")
            time.sleep(1)
            session.host.stop()
            session.host = None
            if self._resources:
                stop_cpu_sampler(target, qnx=True, pidfile=paths.cpu_pidfile)
            else:
                stop_capture(target, qnx=True, log=paths.capture_log)
            session.stopped = True

            result = BenchmarkResult(
                self._transport,
                self._num_clients,
                None,
                None,
                read_summary(sub_summary),
                client_ports=client_ports(self._num_clients),
            )
            if self._resources:
                result.cpu_stats = self._collect_cpu_stats()
                result.mem_stats = self._collect_mem_stats()
            else:
                result.pcap_path = str(self._tmp_path / "sender.pcap")
                result.sendlog_path = str(self._tmp_path / "sendlog.csv")
                assert pull_file(target, paths.pcap, result.pcap_path), "failed to pull sender.pcap"
                assert pull_file(target, paths.sendlog, result.sendlog_path), "failed to pull sendlog.csv"
            return result

    def _launch_sender(self):
        """Start the sender. It offers the service, waits for its grace period, measures and prints BENCHMARK DONE.

        The run settings go in the environment, because the sender reads them only from there.
        """
        repetitions = _RESOURCE_REPETITIONS if self._resources else _REPETITIONS
        return self._launch_app(
            f"SOMEIP_BENCH_SUBSCRIBE_WAIT_MS={_SUBSCRIBE_WAIT_MS} "
            f"SOMEIP_BENCH_REPETITIONS={repetitions} SOMEIP_BENCH_SEND_GAP_US={_SEND_GAP_US} "
            f"SOMEIP_BENCH_SEND_RETRY_MS={_SEND_RETRY_MS} "
            f"SOMEIP_BENCH_SENDLOG={self._paths.sendlog}"
        )

    def _collect_cpu_stats(self) -> list:
        """Copy the sampler log and calculate CPU statistics per process. Empty if there are no samples."""
        local_path = str(self._tmp_path / "cpu.log")
        if not pull_file(self._target, self._paths.cpu_log, local_path):
            return []
        return analyze_cpu_log(local_path, qnx=True, interval_s=_CPU_INTERVAL_S, names=_CPU_TARGETS)

    def _collect_mem_stats(self) -> list:
        """Copy the memory log and calculate memory statistics per process. Empty if there are no samples."""
        local_path = str(self._tmp_path / "mem.log")
        if not pull_file(self._target, self._paths.mem_log, local_path):
            return []
        return analyze_mem_log(local_path)

    def _diagnose_sd(self) -> str:
        """Describe the SD traffic of the target, for a subscriber that did not find the service.

        The capture filter is only the transport, thus it also records SD.
        The count of those frames per destination shows if the service had no offer, or if an offer was lost.
        """

        def count(extra: str) -> str:
            read = f"tcpdump -n -r {self._paths.pcap} 'udp port {SD_PORT}' 2>/dev/null"
            return text(self._target.execute(f"{read}{extra} | wc -l")[1]).strip() or "?"

        total = count("")
        to_group = count(f" and dst host {SD_GROUP}")
        return (
            f"target-side SD frames: {total} total, {to_group} to the SD group {SD_GROUP}.\n"
            f"0 total means that the service had no offer (examine someipd.log and sender.log, not the host).\n"
            "Offers to the group without a subscriber reaction mean that the host does not receive the SD "
            "multicast -- examine its group membership with a capture on the host."
        )


def _log_tail(local_path, label: str) -> None:
    """Log the end of a local log file at ERROR, thus it shows in --test_output=errors."""
    try:
        content = Path(local_path).read_text(errors="replace")
    except OSError:
        return
    tail = "".join(content.splitlines(keepends=True)[-_LOG_TAIL_LINES:])
    _logger.error("%s (last %d lines):\n%s", label, _LOG_TAIL_LINES, tail)
