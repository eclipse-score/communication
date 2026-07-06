# *****************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *****************************************************************************
"""Signal probe: observe which cleanup hooks Bazel triggers when killing a test.

Purpose
-------
This script records which cleanup hooks run depending on how the process is
killed. It is the reproducible evidence base for the score_itf Docker network
orphan bug report.

How to run
----------
Scenario A — Bazel timeout (sends SIGKILL):
    bazel test //quality/integration_testing/test:test_signal_probe --test_timeout=5
    cat /tmp/signal_probe_result.txt

Scenario B — Ctrl+C during bazel test (Bazel sends SIGTERM to subprocess):
    bazel test //quality/integration_testing/test:test_signal_probe
    # Press Ctrl+C after "sleeping..." appears, then:
    cat /tmp/signal_probe_result.txt

Expected results
----------------
Scenario A (SIGKILL — exit code 137):
    result file: only startup lines — no handler ran, no finally, no atexit.

Scenario B (SIGTERM — exit code 143):
    result file: "SIGTERM received" — SIGTERM handler ran.
    NO "finally ran" — os._exit() bypasses Python cleanup, same effect as
    unhandled SIGTERM (which is what docker.py gets, since it has no SIGTERM
    handler).
    NO "atexit ran" — os._exit() bypasses atexit too.

Both scenarios leave the score_itf_* Docker network orphaned because the
finally block in docker.py never runs in either case.
"""

import atexit
import os
import signal
import sys
import time

RESULT_FILE = "/tmp/signal_probe_result.txt"


def _log(msg: str) -> None:
    with open(RESULT_FILE, "a") as f:
        f.write(msg + "\n")
    print(msg, flush=True)


def _sigterm_handler(signum, frame):  # pylint: disable=unused-argument
    # Use os._exit() — NOT sys.exit() — to mirror docker.py which has no
    # SIGTERM handler. sys.exit() would raise SystemExit which WOULD trigger
    # finally and atexit. os._exit() bypasses all Python cleanup, the same
    # end-result as an unhandled SIGTERM.
    _log("SIGTERM received — calling os._exit(143) [finally and atexit will NOT run]")
    os._exit(143)


def _sigint_handler(signum, frame):  # pylint: disable=unused-argument
    _log("SIGINT received — raising KeyboardInterrupt [finally and atexit WILL run]")
    raise KeyboardInterrupt


def _atexit_hook():
    _log("atexit ran")


# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------
open(RESULT_FILE, "w").close()
_log(f"started (PID={os.getpid()})")
_log("registering: SIGTERM -> os._exit(143), SIGINT -> KeyboardInterrupt, atexit hook")

signal.signal(signal.SIGTERM, _sigterm_handler)
signal.signal(signal.SIGINT, _sigint_handler)
atexit.register(_atexit_hook)

_log("all handlers registered")

# ---------------------------------------------------------------------------
# Main sleep — kill this process now to observe behaviour
# ---------------------------------------------------------------------------
try:
    _log("sleeping 120 s — kill this process to observe signal behaviour")
    time.sleep(120)
finally:
    _log("finally ran — only printed on clean exit or KeyboardInterrupt (SIGINT)")

_log("normal exit")
sys.exit(0)

"""Signal probe: observe which cleanup hooks Bazel triggers when killing a test.

Purpose
-------
This test exists solely to answer: "which signal does Bazel send to a test
process on timeout / Ctrl+C, and do Python finally-blocks or atexit handlers
still run?"  It is the reproducible evidence base for the score_itf Docker
network orphan bug report.

How to run
----------
Scenario A — Bazel timeout (SIGKILL):
    bazel test //quality/integration_testing/test:test_signal_probe --test_timeout=5
    # After it is killed, inspect the result file:
    cat /tmp/signal_probe_result.txt
    docker network ls --filter "name=score_itf_"

Scenario B — User Ctrl+C (Bazel sends SIGTERM to subprocess):
    bazel test //quality/integration_testing/test:test_signal_probe
    # Press Ctrl+C after "sleeping..." appears in the output, then:
    cat /tmp/signal_probe_result.txt
    docker network ls --filter "name=score_itf_"

Expected results
----------------
Scenario A (SIGKILL):
  signal_probe_result.txt contains only "started" lines — no handler fired, no
  finally ran, no atexit ran.
  Docker network is still present (orphaned).

Scenario B (SIGTERM):
  signal_probe_result.txt shows "SIGTERM received" but NOT "finally ran" — Python
  has no default SIGTERM handler so the process exits without running finally.
  Docker network is still present (orphaned).

Both scenarios leave an orphaned score_itf_* network, which is the root cause of
accumulation after repeated integration test runs.
"""

import atexit
import os
import signal
import sys
import time

try:
    import docker as _docker
    _docker_available = True
except ImportError:
    _docker_available = False

RESULT_FILE = "/tmp/signal_probe_result.txt"


def _log(msg: str) -> None:
    """Write to both stdout (visible in --test_output=streamed) and result file."""
    line = msg
    with open(RESULT_FILE, "a") as f:
        f.write(line + "\n")
    print(line, flush=True)


def _sigterm_handler(signum, frame):  # pylint: disable=unused-argument
    _log("SIGTERM received — calling sys.exit(143)")
    sys.exit(143)


def _sigint_handler(signum, frame):  # pylint: disable=unused-argument
    _log("SIGINT received — calling sys.exit(130)")
    sys.exit(130)


def _atexit_hook():
    _log("atexit ran")


# ---------------------------------------------------------------------------
# Module-level setup so handlers are registered as soon as pytest imports us
# ---------------------------------------------------------------------------
open(RESULT_FILE, "w").close()  # clear previous run
_log(f"module loaded (PID={os.getpid()})")
_log("registering: SIGTERM handler, SIGINT handler, atexit hook")

signal.signal(signal.SIGTERM, _sigterm_handler)
signal.signal(signal.SIGINT, _sigint_handler)
atexit.register(_atexit_hook)

_log("all handlers registered")


# ---------------------------------------------------------------------------
# The actual pytest test function
# ---------------------------------------------------------------------------
def test_signal_behavior() -> None:
    """Sleep for 120 s. This test is MEANT to be killed externally.

    See module docstring for how to run the two scenarios.
    """
    _log("test_signal_behavior entered — creating Docker network")

    net = None
    if _docker_available:
        client = _docker.from_env()
        net = client.networks.create(
            f"score_itf_{os.urandom(8).hex()}", driver="bridge"
        )
        _log(f"Docker network created: {net.name}")
        _log("(if this process is killed without finally running, the network above will be orphaned)")
    else:
        _log("docker SDK not available — skipping network creation")

    try:
        _log("sleeping 120 s — kill this process now to observe signal behaviour")
        time.sleep(120)
    finally:
        _log("finally ran — this only prints if the process exits cleanly")
        if net is not None:
            try:
                net.remove()
                _log(f"Docker network removed: {net.name}")
            except Exception as exc:  # pylint: disable=broad-except
                _log(f"Docker network removal failed: {exc}")
