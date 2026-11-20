#!/usr/bin/env python3
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

"""Capture packets and sample %CPU and memory per process on an ITF target. Parse the sampler logs on the host.

Each function that runs an OS-specific tool on the target has a ``qnx`` argument.
QNX has no ``pkill``, ``pgrep`` or ``top -b``.
The Linux sampler is ``top -b`` on the given pids. The QNX sampler is a detached ``pidin times`` loop, matched by name.
Only QNX samples memory, in the same loop, from ``/proc/<pid>/vmstat`` of each given pid.
The parsers use only the standard library. Thus they also run where ITF is not installed.

Usage:
    python3 -m perf_itf.sampling --log cpu.log [--qnx --interval 0.5 --names a,b]
    python3 -m perf_itf.sampling --selftest
"""

from __future__ import annotations

import argparse
import contextlib
import itertools
import logging
import re
import shlex
import sys
import tempfile
import time
from collections import defaultdict
from collections.abc import Sequence
from dataclasses import dataclass

from perf_itf.process import kill_by_name
from perf_itf.shell import sh, text
from perf_itf.waiting import wait_for_log_marker

_logger = logging.getLogger("perf_itf.sampling")

PIDMAP_PREFIX = "# pidmap"
PAGESIZE_PREFIX = "# pagesize"
TS_PREFIX = "TS "

# A procps top -b process row is PID USER PR NI VIRT RES SHR S %CPU %MEM TIME+ COMMAND.
# Thus the %CPU column is index 8 after a split on whitespace.
_TOP_MIN_FIELDS = 12
_TOP_CPU_FIELD = 8


def _sh(target, cmd: str) -> str:
    """Return the decoded output of *cmd*, without an exit-code check. A sampler tool can print and fail."""
    return sh(target, cmd, check=False, merge_stderr=False)


# Kernel capture buffer for tcpdump, in KiB: one second of full-payload traffic of a 32 KB bucket.
# The QNX io-sock limits a BPF buffer to net.bpf.maxbufsize, 512 KiB by default. That traffic fills it in approximately
# 30 ms. Thus the capture first sets the limit above the request. The setting is lost after a reboot.
_CAPTURE_BUFFER_KIB = 16 * 1024
_QNX_BPF_MAX_BUFFER_BYTES = 32 * 1024 * 1024


def disable_tso(target, *, iface: str) -> bool:
    """Set TCP segmentation offload on *iface* to off. Return True if it was on and is now off.

    With TSO, a capture on the sender host sees each TCP message as one frame, before the NIC segments it.
    A NIC without TSO, or a driver that does not accept the change, returns False. Only the refusal goes to the log.
    """
    if "TSO" not in _sh(target, f"ifconfig {iface}"):
        return False
    code, output = target.execute(f"ifconfig {iface} -tso")
    if code != 0:
        _logger.warning("ifconfig %s -tso failed (%s): %s", iface, code, text(output).strip())
        return False
    return True


def enable_tso(target, *, iface: str) -> None:
    """Set TCP segmentation offload on *iface* to on again, after :func:`disable_tso` returned True."""
    code, output = target.execute(f"ifconfig {iface} tso")
    if code != 0:
        _logger.warning("ifconfig %s tso failed (%s): %s", iface, code, text(output).strip())


def start_capture(target, *, qnx: bool, iface: str, bpf: str, pcap: str, log: str, cpu: int | None = None):
    """Start tcpdump on *iface*. It writes the packets that match *bpf* to *pcap*. Return its ITF async handle.

    Returns when tcpdump reports that it listens. Thus it captures all packets sent after the call.
    Timestamps are ``CLOCK_REALTIME`` with nanosecond precision.
    tcpdump writes the pcap when :func:`stop_capture` stops it, not per packet.
    Thus a capture that stops in a different way can end with a truncated record.
    """
    # -Z root is only for the container. The Linux image installs tcpdump from apt. Its 'tcpdump' user cannot write
    # the pcap path.
    # With exec, the ITF handle has the tcpdump pid. Thus proc.stop() stops tcpdump.
    privilege_opt = "" if qnx else "-Z root "
    # QNX 'on -C' pins tcpdump to *cpu*. It execs tcpdump, thus the pid stays.
    pin = f"on -C {cpu} " if qnx and cpu is not None else ""
    raise_cap = f"sysctl -w net.bpf.maxbufsize={_QNX_BPF_MAX_BUFFER_BYTES} > /dev/null; " if qnx else ""
    proc = target.execute_async(
        f"{raise_cap}exec {pin}tcpdump -i {iface} --time-stamp-precision=nano -n -s 0 -B {_CAPTURE_BUFFER_KIB} "
        f"{privilege_opt}-w {pcap} {bpf} > {log} 2>&1"
    )
    try:
        wait_for_log_marker(target, log, "listening on", 15, "tcpdump")
    except BaseException:
        # A capture that did not listen is still a process that runs, and on SSH an open session.
        with contextlib.suppress(Exception):
            kill_by_name(target, qnx=qnx, names=("tcpdump",))
        with contextlib.suppress(Exception):
            proc.stop()
        raise
    return proc


def stop_capture(target, *, qnx: bool, log: str, wait_for_flush: bool = True) -> None:
    """Send SIGTERM to tcpdump. If *wait_for_flush* is true, wait for its "packets captured" summary.

    The summary shows that the pcap is complete. Thus wait for it only before you copy the pcap.
    After a capture that did not start, the wait uses all of the timeout for a marker that cannot appear.
    """
    kill_by_name(target, qnx=qnx, names=("tcpdump",))
    if wait_for_flush:
        wait_for_log_marker(target, log, "packets captured", 20, "tcpdump flush")


def resolve_pids(target, *, qnx: bool, names: Sequence[str]) -> list[tuple[str, int]]:
    """Find the pid(s) of each name in *names*, as ``(name, pid)`` pairs.

    ``execute_async`` returns before the binary exec. Thus try each name again for a short time until its pid appears.
    On Linux, the pgrep pattern starts at the start of the command line, as ``<name>`` or ``.../<name>``.
    Thus it cannot match the ``/bin/sh -c`` wrapper of the command.
    QNX has no pgrep. Thus it matches the ``/<name>`` end of the ``pidin`` ``%n`` path, one row for each thread.
    ``%N`` is not sufficient. It keeps only the last 18 characters of a name.
    """
    procs: list[tuple[str, int]] = []
    for name in names:
        deadline = time.monotonic() + 3.0
        while True:
            if qnx:
                rows = (line.split() for line in _sh(target, 'pidin -F "%a %n"').splitlines())
                found = sorted({row[0] for row in rows if len(row) == 2 and row[1].endswith(f"/{name}")})
            else:
                found = [tok for tok in _sh(target, f"pgrep -f '^([^ ]*/)?{name}( |$)'").split() if tok.isdigit()]
            if found or time.monotonic() >= deadline:
                break
            time.sleep(0.1)
        procs.extend((name, int(pid)) for pid in found)
    return procs


def start_cpu_sampler(
    target,
    *,
    qnx: bool,
    procs: Sequence[tuple[str, int]],
    log: str,
    pidfile: str,
    interval_s: float,
    mem_log: str | None = None,
):
    """Start to sample %CPU of *procs* (``(name, pid)`` pairs) into *log*. On QNX, also sample memory into *mem_log*.

    Returns the ITF handle on Linux. Returns None on QNX or if *procs* is empty.
    Linux runs ``top -b`` on the pids, after a ``# pidmap`` header. top truncates names to 15 characters.
    QNX runs a detached ``pidin times`` loop named ``sh``, the same name as the shells of the run.
    Thus its pid goes to *pidfile*.
    With *mem_log* and *procs*, the QNX loop also copies ``/proc/<pid>/vmstat`` of each pid into *mem_log*.
    Raises RuntimeError if the QNX loop cannot start or stops immediately. Thus a run never silently has no samples.
    """
    if qnx:
        mem_sample = ""
        if mem_log and procs:
            header = shlex.quote(PIDMAP_PREFIX + " " + " ".join(f"{name}={pid}" for name, pid in procs))
            _sh(target, f"printf '%s\\n' {header} \"{PAGESIZE_PREFIX} $(getconf PAGESIZE)\" > {mem_log}")
            vmstats = " ".join(f"/proc/{pid}/vmstat" for _name, pid in procs)
            # The stderr of the loop is the CPU log. Thus a vmstat file of a stopped process must not report there.
            mem_sample = f"grep -HE 'as_stats\\.(rss|map_private)=' {vmstats} >>{mem_log} 2>/dev/null; "
        # nohup: the QNX SSH session stays open until all started processes exit.
        # Thus with a usual background job, the call does not return.
        # Each round of the loop takes longer than *interval_s*, more on a board with a high load.
        # Thus each round has a wall clock stamp in nanoseconds, and the parser divides by the real elapsed time.
        loop = f"while true; do printf 'TS %s\\n' \"$(date +%s.%N)\"; pidin times; {mem_sample}sleep {interval_s}; done"
        # /bin/sh with its path. nohup finds its command through PATH. On the qemu image, PATH has only /proc/boot.
        code, output = target.execute(
            f"(nohup /bin/sh -c {shlex.quote(loop)} </dev/null >{log} 2>&1 & echo $! > {pidfile})"
        )
        if code != 0:
            raise RuntimeError(f"could not spawn the pidin sampler ({code}): {text(output)}")
        time.sleep(0.2)
        # The start above succeeds also if nohup cannot run its command. Thus make sure that the loop runs.
        if target.execute(f"kill -0 $(cat {pidfile})")[0] != 0:
            raise RuntimeError(f"the pidin sampler exited right after starting: {_sh(target, f'tail -n 5 {log}')}")
        return None
    if not procs:
        return None
    header = shlex.quote(PIDMAP_PREFIX + " " + " ".join(f"{name}={pid}" for name, pid in procs))
    pids = ",".join(str(pid) for _name, pid in procs)
    return target.execute_async(
        f"printf '%s\\n' {header} > {log}; exec top -b -d {interval_s} -w512 -p {pids} >> {log} 2>&1"
    )


def stop_cpu_sampler(target, *, qnx: bool, pidfile: str) -> None:
    """Stop the sampler of :func:`start_cpu_sampler`. Then its log is complete. A second call has no effect."""
    if qnx:
        _sh(
            target,
            f"kill $(cat {pidfile}) 2>/dev/null; slay -f -Q pidin grep 2>/dev/null; rm -f {pidfile} 2>/dev/null; true",
        )
    else:
        # The brackets prevent a signal to the /bin/sh -c wrapper that runs pkill.
        _sh(target, "pkill -f '[t]op -b' || true")


def parse_top(text: str) -> dict[str, list[float]]:
    """Map the process name to its %CPU samples, from ``top -b`` output after a ``# pidmap`` header.

    A log without the header gives nothing, because the top rows do not have the full name.
    """
    pidmap: dict[int, str] = {}
    samples: dict[str, list[float]] = defaultdict(list)
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith(PIDMAP_PREFIX):
            for tok in stripped[len(PIDMAP_PREFIX) :].split():
                name, sep, pid = tok.partition("=")
                if sep:
                    with contextlib.suppress(ValueError):
                        pidmap[int(pid)] = name
            continue
        parts = stripped.split()
        if len(parts) < _TOP_MIN_FIELDS:
            continue
        try:
            pid = int(parts[0])
        except ValueError:
            continue  # header ("PID ...") and summary lines
        name = pidmap.get(pid)
        if name is None:
            continue
        try:
            cpu = float(parts[_TOP_CPU_FIELD].replace(",", "."))  # locale comma
        except ValueError:
            continue
        samples[name].append(cpu)
    return dict(samples)


def _trailing_seconds(tokens: list[str]) -> list[float]:
    """Collect the last decimal-second columns of a ``pidin times`` line: utime, stime, cutime, cstime.

    The scan starts at the end and stops at the date column. Its ``HH:MM`` clock is not a float.
    Thus the scan does not get to the pid and ppid integers at the start.
    """
    secs: list[float] = []
    for tok in reversed(tokens):
        try:
            secs.append(float(tok))
        except ValueError:
            break
    secs.reverse()
    return secs


def _pidin_snapshot(block: str, names: Sequence[str]) -> dict[str, float]:
    """Return the cumulative CPU ms (utime + stime) per name in one pidin snapshot. Use the largest row of a process."""
    out: dict[str, float] = {}
    for line in block.splitlines():
        for name in names:
            if name not in line:
                continue
            secs = _trailing_seconds(line.split())
            if secs:
                # Only utime and stime. The child times include the work of child processes.
                cpu_ms = sum(secs[:2]) * 1000.0
                out[name] = max(out.get(name, 0.0), cpu_ms)
            break
    return out


def parse_pidin(text: str, interval_s: float, names: Sequence[str]) -> dict[str, list[float]]:
    """Map the process name to its %CPU samples, from ``pidin times`` snapshots between ``TS`` lines.

    Each sample is the change of the cumulative CPU time, divided by the wall time between the two ``TS`` stamps.
    If one of the two stamps is not in fractional seconds (``date`` without ``%N``), use *interval_s*,
    the configured interval.
    A name that is not in the two snapshots of a pair gives no sample.
    """
    if interval_s <= 0:
        return {}
    blocks: list[tuple[float | None, str]] = []
    stamp: float | None = None
    cur: list[str] | None = None
    for line in text.splitlines():
        if line.startswith(TS_PREFIX):
            if cur is not None:
                blocks.append((stamp, "\n".join(cur)))
            stamp = _fractional_stamp(line[len(TS_PREFIX) :])
            cur = []
        elif cur is not None:
            cur.append(line)
    if cur is not None:
        blocks.append((stamp, "\n".join(cur)))

    snaps = [(ts, _pidin_snapshot(b, names)) for ts, b in blocks]
    samples: dict[str, list[float]] = defaultdict(list)
    for (prev_ts, prev), (next_ts, nxt) in itertools.pairwise(snaps):
        elapsed_s = interval_s
        if prev_ts is not None and next_ts is not None and next_ts > prev_ts:
            elapsed_s = next_ts - prev_ts
        for name, cum in nxt.items():
            if name in prev:
                delta = cum - prev[name]
                if delta >= 0.0:
                    samples[name].append(delta / (elapsed_s * 1000.0) * 100.0)
    return dict(samples)


def _fractional_stamp(field: str) -> float | None:
    """Return the seconds of a ``<s>.<ns>`` ``TS`` stamp, or None for whole seconds or an unexpanded ``%N``."""
    field = field.strip()
    if "." not in field:
        return None
    try:
        return float(field)
    except ValueError:
        return None


@dataclass
class CpuStat:
    process: str
    count: int
    mean: float
    peak: float


def aggregate(samples: dict[str, list[float]]) -> list[CpuStat]:
    """Reduce the %CPU samples of each process to count, mean and peak. A name without samples gets a zero row."""
    out: list[CpuStat] = []
    for name in sorted(samples):
        vals = samples[name]
        if not vals:
            out.append(CpuStat(name, 0, 0.0, 0.0))
            continue
        out.append(CpuStat(name, len(vals), sum(vals) / len(vals), max(vals)))
    return out


def render_cpu_table(stats: list[CpuStat]) -> str:
    """Show %CPU per process as a fixed-width table. %CPU is per core, thus a multi-thread process can be above 100."""
    hdr = f"{'process':<28}{'samples':>9}{'mean %CPU':>12}{'peak %CPU':>12}"
    lines = [hdr, "-" * len(hdr)]
    for s in stats:
        lines.append(f"{s.process:<28}{s.count:>9}{s.mean:>12.1f}{s.peak:>12.1f}")
    if not stats:
        lines.append("(no CPU samples collected)")
    return "\n".join(lines)


def analyze_cpu_log(path: str, *, qnx: bool, interval_s: float, names: Sequence[str]) -> list[CpuStat]:
    """Parse a sampler log of :func:`start_cpu_sampler` and calculate %CPU per process.

    On QNX, *names* selects the rows. On Linux, the pidmap header in the log gives the names.
    """
    with open(path, errors="replace") as fh:
        text = fh.read()
    samples = parse_pidin(text, interval_s, names) if qnx else parse_top(text)
    return aggregate(samples)


# The size in parentheses after the hex page count is in kB or MB. Thus only the page count is parsed.
_VMSTAT_LINE = re.compile(r"^/proc/(\d+)/vmstat:as_stats\.(rss|map_private)=0x([0-9a-fA-F]+)")

_MIB = 1024.0 * 1024.0


@dataclass
class MemStat:
    process: str
    count: int
    mean_rss: float
    peak_rss: float
    peak_private: float


def analyze_mem_log(path: str) -> list[MemStat]:
    """Calculate memory statistics per process in MiB from a QNX memory log of :func:`start_cpu_sampler`.

    RSS counts all resident pages mapped into the process, which includes shared libraries and shared memory.
    Private memory counts only the mappings of the process. Thus you can add it across processes.
    The ``# pidmap`` header gives the pid names. The ``# pagesize`` header gives the page size, 4 KiB if not readable.
    """
    pidmap: dict[int, str] = {}
    pagesize = 4096
    samples: dict[str, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    with open(path, errors="replace") as fh:
        for line in fh:
            if line.startswith(PIDMAP_PREFIX):
                pidmap = {int(pid): name for name, _, pid in (tok.partition("=") for tok in line.split()[2:])}
            elif line.startswith(PAGESIZE_PREFIX):
                with contextlib.suppress(ValueError, IndexError):
                    pagesize = int(line.split()[2])
            elif (match := _VMSTAT_LINE.match(line)) and int(match.group(1)) in pidmap:
                name = pidmap[int(match.group(1))]
                samples[name][match.group(2)].append(int(match.group(3), 16) * pagesize / _MIB)
    return [
        MemStat(name, len(rss), sum(rss) / len(rss), max(rss), max(fields["map_private"], default=0.0))
        for name, fields in sorted(samples.items())
        if (rss := fields["rss"])
    ]


def render_mem_table(stats: list[MemStat]) -> str:
    """Show memory per process as a fixed-width table in MiB."""
    hdr = f"{'process':<28}{'samples':>9}{'mean RSS MiB':>15}{'peak RSS MiB':>15}{'peak private MiB':>19}"
    lines = [hdr, "-" * len(hdr)]
    for s in stats:
        lines.append(f"{s.process:<28}{s.count:>9}{s.mean_rss:>15.1f}{s.peak_rss:>15.1f}{s.peak_private:>19.1f}")
    if not stats:
        lines.append("(no memory samples collected)")
    return "\n".join(lines)


_TOP_SAMPLE = """\
# pidmap someipd=123 someip_benchmark_sender=456
top - 09:04:52 up  3:00,  0 user,  load average: 0.44, 0.19, 0.08
Tasks:   3 total,   1 running,   2 sleeping,   0 stopped,   0 zombie
%Cpu(s):  1.0 us,  0.5 sy,  0.0 ni, 98.1 id,  0.0 wa,  0.0 hi,  0.5 si,  0.0 st
MiB Mem :  40106.6 total,  15551.5 free,   8129.5 used,  17067.0 buff/cache
MiB Swap:   8192.0 total,   8192.0 free,      0.0 used.  31977.1 avail Mem

    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
    123 root      20   0  123456  45678  12345 S  42.0   0.1   0:01.23 someipd
    456 root      20   0   23456   6789   3456 S   8.0   0.0   0:00.45 someip_benchma

    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
    123 root      20   0  123456  45678  12345 R  60.0   0.1   0:01.83 someipd
    456 root      20   0   23456   6789   3456 S  12.0   0.0   0:00.57 someip_benchma
"""

# Two snapshots, 0.5 s apart, in the QNX ``pidin times`` line format:
#   <pid> <name> <ppid> <date> <utime> <stime> <cutime> <cstime>, with the four times in seconds.
_PIDIN_SAMPLE = """\
TS 1718990000
73650208 ./someipd                    73646110 Jan 01 01:09  0.150  0.050  0.000  0.000
73781282 /bin/someip_benchmark_sender 73777182 Jan 01 01:09  0.020  0.010  0.000  0.000
TS 1718990001
73650208 ./someipd                    73646110 Jan 01 01:09  0.400  0.100  0.000  0.000
73781282 /bin/someip_benchmark_sender 73777182 Jan 01 01:09  0.050  0.030  0.000  0.000
"""


# The same snapshots, with ``date`` stamps with ``%N``, 1.25 s apart.
_PIDIN_STAMPED_SAMPLE = _PIDIN_SAMPLE.replace("TS 1718990000\n", "TS 1718990000.250000000\n").replace(
    "TS 1718990001\n", "TS 1718990001.500000000\n"
)

# Two QNX vmstat samples of one process, after the start_cpu_sampler headers. Pid 99 is not in the pidmap.
_MEM_SAMPLE = """\
# pidmap someipd=123
# pagesize 4096
/proc/123/vmstat:as_stats.rss=0x100 (1024kB)
/proc/123/vmstat:as_stats.map_private=0x80 (512kB)
/proc/99/vmstat:as_stats.rss=0x999 (9828kB)
/proc/123/vmstat:as_stats.rss=0x300 (3MB)
/proc/123/vmstat:as_stats.map_private=0x40 (256kB)
"""


def selftest() -> int:
    """Check the parsers with fixed sampler output. 0 is pass."""
    names = ("someipd", "someip_benchmark_sender")

    top = parse_top(_TOP_SAMPLE)
    assert top["someipd"] == [42.0, 60.0], top
    assert top["someip_benchmark_sender"] == [8.0, 12.0], top
    top_stats = {s.process: s for s in aggregate(top)}
    assert top_stats["someipd"].count == 2
    assert top_stats["someipd"].peak == 60.0
    assert abs(top_stats["someipd"].mean - 51.0) < 1e-9

    pid = parse_pidin(_PIDIN_SAMPLE, interval_s=0.5, names=names)
    # someipd: utime + stime changes from 0.200 to 0.500 s. 0.300 s in 0.5 s is 60%.
    assert abs(pid["someipd"][0] - 60.0) < 1e-6, pid
    # sender: utime + stime changes from 0.030 to 0.080 s. 0.050 s in 0.5 s is 10%.
    assert abs(pid["someip_benchmark_sender"][0] - 10.0) < 1e-6, pid
    assert parse_pidin(_PIDIN_SAMPLE, interval_s=0.0, names=names) == {}
    # With stamps 1.25 s apart, the 0.300 s of someipd is 24%, independent of the configured interval.
    stamped = parse_pidin(_PIDIN_STAMPED_SAMPLE, interval_s=0.5, names=names)
    assert abs(stamped["someipd"][0] - 24.0) < 1e-6, stamped

    with tempfile.NamedTemporaryFile("w", suffix=".log") as fh:
        fh.write(_MEM_SAMPLE)
        fh.flush()
        mem = analyze_mem_log(fh.name)
    # 0x100 and 0x300 pages of 4 KiB are 1 and 3 MiB. Peak private is 0x80 pages, 0.5 MiB.
    assert [(m.process, m.count, m.mean_rss, m.peak_rss, m.peak_private) for m in mem] == [
        ("someipd", 2, 2.0, 3.0, 0.5)
    ], mem

    print("perf_itf.sampling selftest OK")
    print(render_cpu_table(aggregate(top)))
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--log", help="sampler log file (top or pidin output)")
    ap.add_argument("--qnx", action="store_true", help="the log is QNX pidin output rather than Linux top output")
    ap.add_argument("--interval", type=float, default=0.5, help="sample interval in seconds (QNX %%CPU derivation)")
    ap.add_argument("--names", default="", help="comma-separated process names to report (QNX)")
    ap.add_argument("--selftest", action="store_true", help="run the built-in checks and exit")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    if not args.log:
        ap.error("--log is required (or use --selftest)")
    names = [n for n in args.names.split(",") if n]
    if args.qnx and not names:
        ap.error("--names is required with --qnx, since pidin rows are matched by name")
    print(render_cpu_table(analyze_cpu_log(args.log, qnx=args.qnx, interval_s=args.interval, names=names)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
