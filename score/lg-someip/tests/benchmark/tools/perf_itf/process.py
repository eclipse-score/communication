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

"""Helpers that examine and kill processes on an ITF target, on Linux or QNX.

The docker backend target is Linux and uses ``pkill``/``pgrep``. The qemu backend target is QNX and uses
``slay``/``pidin``, which have different options. QNX images often link ``pkill`` to ``slay``, but it is not
compatible. On such an image, ``pkill -KILL -x someip_dut`` prints ``slay: Invalid option or missing argument
(-K)`` and kills nothing. A caller that only examines the exit code sees success.

Thus each function here examines the effect, not the exit status.
:func:`kill_process` raises an error if the process continues to run after the signal.
"""

from __future__ import annotations

import time
from collections.abc import Sequence

from perf_itf.shell import text

# QNX `pidin -F %N` pads the name to a fixed column width. Thus the first field is the name.
# There is one line per thread, thus sort -u.
_QNX_LIST_NAMES = "pidin -F \"%N\" | awk '{print $1}' | sort -u"


def is_qnx(target) -> bool:
    """True if the target itself reports that *target* runs QNX.

    This is better than the pytest backend option. A suite gets the same answer if the platform, the plugin or
    a configuration file selects it.
    """
    exit_code, output = target.execute("uname -s")
    return exit_code == 0 and text(output).strip() == "QNX"


def process_names(target) -> set[str]:
    """All names of the processes that run on *target* now."""
    if is_qnx(target):
        exit_code, output = target.execute(_QNX_LIST_NAMES)
    else:
        exit_code, output = target.execute("ps -eo comm= | sort -u")
    if exit_code != 0:
        raise RuntimeError(f"could not list processes on the target: {text(output)}")
    return {line.strip() for line in text(output).splitlines() if line.strip()}


def is_running(target, name: str) -> bool:
    """True if a process with the exact name *name* runs on *target*."""
    return name in process_names(target)


def process_running(target, name: str) -> bool:
    """True if the binary *name* runs on *target*. The match uses the start path, not the process name.

    Linux truncates /proc/PID/comm to 15 characters, which is too short for some binary names.
    The bracket prevents a pgrep match on the shell that runs this command line.
    For the same reason, the QNX fallback is a second command. With ||, pgrep matches the name without brackets
    in the arguments of the shell.
    """
    if target.execute(f"pgrep -f '[{name[0]}]{name[1:]}' >/dev/null 2>&1")[0] == 0:
        return True
    # QNX has no pgrep. The pidin name column is the binary path without the first slash, padded, below a header.
    strip = "tail -n +2 | sed 's/ *$//'"
    return target.execute(f"pidin -F \"%n\" 2>/dev/null | {strip} | grep -q -e '^{name}$' -e '/{name}$'")[0] == 0


def kill_process(target, name: str, timeout: float = 5.0) -> None:
    """Send SIGKILL to each process with the exact name *name* on *target*, and wait until it stops.

    The match uses the exact name. Thus a kill of ``someip_dut`` does not stop ``someipd``.
    A suite that monitors the expiry of the offer TTL needs this.

    :raises RuntimeError: the process runs *timeout* seconds after the signal, or it did not run at the start.
        In the two cases, the next step of the caller uses an incorrect model of the target.
    """
    if not is_running(target, name):
        raise RuntimeError(f"no process named {name!r} on the target, so nothing was killed")

    if is_qnx(target):
        # slay returns a value that is not zero, also when it kills the process. Thus the exit code is not useful.
        # The poll below finds the result.
        target.execute(f"slay -s SIGKILL -f {name}")
    else:
        target.execute(f"pkill -KILL -x {name}")

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not is_running(target, name):
            return
        time.sleep(0.05)
    raise RuntimeError(f"{name} still running {timeout}s after SIGKILL")


def _name_pattern(name: str) -> str:
    """Return the ``pkill -f`` regex for a command line with program *name*, as ``<name>`` or ``.../<name>``."""
    return f"(^|/){name}( |$)"


def kill_by_name(target, *, qnx: bool, names: Sequence[str], signal_name: str = "TERM") -> None:
    """Send a signal to each process with a name in *names*. A name without a match is not an error.

    On Linux, the match uses the command line, as ``<name>`` or ``.../<name>``.
    The pkill name match stops at 15 characters.
    QNX uses ``slay -f``. Without ``-f``, slay asks for input when many processes have the same name.
    This stops an SSH run.
    When this function returns, the signal is only in the queue. :func:`wait_until_gone` makes sure that it arrived.
    """
    if qnx:
        # slay accepts many names. pkill accepts only one pattern and fails with more.
        target.execute(f"slay -f -Q -s SIG{signal_name} {' '.join(names)} 2>/dev/null; true")
    else:
        for name in names:
            # The pattern text has ")" before the name. Thus it cannot match the shell that runs this pkill.
            target.execute(f"pkill -{signal_name} -f '{_name_pattern(name)}' || true")


def _running_by_name(target, *, qnx: bool, names: Sequence[str]) -> list[str]:
    if qnx:
        alive = process_names(target)
        return [name for name in names if name in alive]
    # pgrep uses the same pattern as the kill. Thus it cannot match the command line of the poll shell.
    alive = []
    for name in names:
        code, output = target.execute(f"pgrep -f '{_name_pattern(name)}' 2>&1 >/dev/null")
        if code == 0:
            alive.append(name)
        elif code != 1:
            # 1 is no match. Other values: pgrep does not accept the pattern, or pgrep is not on the target.
            raise RuntimeError(f"could not look for {name!r} on the target ({code}): {text(output)}")
    return alive


def wait_until_gone(target, *, qnx: bool, names: Sequence[str], timeout: float = 5.0) -> None:
    """Wait until no process with a name in *names* runs on *target*.

    :raises RuntimeError: a process still runs after *timeout*. The message has its name.
        Thus a caller that wants to use resources of those processes, such as a port or the deploy tree, stops.
    """
    deadline = time.monotonic() + timeout
    while True:
        alive = _running_by_name(target, qnx=qnx, names=names)
        if not alive:
            return
        if time.monotonic() >= deadline:
            raise RuntimeError(f"still running {timeout}s after the signal: {alive}")
        time.sleep(0.05)


def kill_process_if_running(target, name: str, timeout: float = 5.0) -> bool:
    """Send SIGKILL to *name* on *target* if it runs. Return True if it ran.

    For callers that clean up after a previous test. For them, a process that does not run is normal.
    :func:`kill_process` treats this case as an incorrect target model.
    """
    if not is_running(target, name):
        return False
    kill_process(target, name, timeout)
    return True


def thread_names(target, pid: int) -> list[str]:
    """Return all threads of *pid*, one entry for each thread, with the name that the thread set.

    Linux keeps the names in ``/proc/<pid>/task/*/comm``. QNX has no ``task`` directory and gives them in
    the ``%h`` field of ``pidin``. A thread that did not call ``pthread_setname_np`` also has an entry.
    Thus the length is the thread count.

    :raises RuntimeError: the thread list is not readable. This is not the same as a process with no threads.
    """
    if is_qnx(target):
        # One row for each thread, "<tid> <name>", after a header line. A thread without a name shows its tid.
        exit_code, output = target.execute(f"pidin -p {pid} -F \"%b %h\" | awk 'NR > 1 {{print $2}}'")
    else:
        exit_code, output = target.execute(f"cat /proc/{pid}/task/*/comm")
    if exit_code != 0:
        raise RuntimeError(f"could not read the threads of pid {pid}: {text(output)}")
    return [line.strip() for line in text(output).splitlines() if line.strip()]


def rss_kb(target, pid: int) -> int:
    """Return the resident set size of *pid* in kB, or -1 if it is not readable.

    Linux gives it as ``VmRSS`` in ``/proc/<pid>/status``. QNX does not have this file.
    QNX gives ``as_stats.rss`` in ``/proc/<pid>/vmstat`` as a hex page count.
    The page size changes it to the same unit.
    """
    if is_qnx(target):
        exit_code, output = target.execute(
            f"awk -F'[=( ]' '/as_stats.rss/{{print $2}}' /proc/{pid}/vmstat; getconf PAGESIZE"
        )
        if exit_code != 0:
            return -1
        fields = text(output).split()
        if len(fields) < 2:
            return -1
        try:
            return int(fields[0], 16) * int(fields[1]) // 1024
        except ValueError:
            return -1

    exit_code, output = target.execute(f"awk '/VmRSS/{{print $2}}' /proc/{pid}/status")
    if exit_code != 0:
        return -1
    try:
        return int(text(output).strip())
    except ValueError:
        return -1
