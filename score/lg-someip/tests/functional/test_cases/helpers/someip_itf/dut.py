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
"""Start the lg-someip routing daemon and the test apps on an ITF docker target.

The image has the binaries and the configs under :data:`ROOT`.
Each config has a placeholder unicast address. The helpers replace it with the target address before start.
lg-someip starts only when ``unicast`` is an address of a local interface.
"""

from __future__ import annotations

import contextlib
import logging
import os
import tempfile
import time
from typing import Final

logger = logging.getLogger(__name__)

ROOT: Final = "/opt/someip_test"
"""The directory of the daemon, the test apps (``bin``) and their configs (``etc``)."""

RUN_DIR: Final = "/tmp/someip_test"
"""The directory of the configs with the target address."""

BUILD_IP: Final = "160.48.201.101"
"""The placeholder unicast address the configs are built with."""

SOCKET_GLOB: Final = "/tmp/someip/someip-*"
"""The local sockets of the daemon. Apps cannot register before the first socket exists."""

ETS: Final = "ets"
TC8: Final = "tc8"


def config_path(app: str) -> str:
    """The patched config of *app* on the target."""
    return f"{RUN_DIR}/{app}_lgsomeip_config.json"


def provision_configs(target, host_ip: str) -> None:
    """Write each config under ``ROOT/etc`` to :data:`RUN_DIR`, with *host_ip* in place of :data:`BUILD_IP`.

    The host changes the files, because the integration-test image has no sed.
    """
    target.execute(f"mkdir -p {RUN_DIR}")
    for app in (ETS, TC8):
        name = f"{app}_lgsomeip_config.json"
        exit_code, output = target.execute(f"cat {ROOT}/etc/{name}")
        if exit_code != 0:
            continue  # this image has only the app of the other family
        text = output.decode()
        assert BUILD_IP in text, f"{name} does not carry {BUILD_IP}"
        with tempfile.TemporaryDirectory() as tmp:
            local = os.path.join(tmp, name)
            with open(local, "w", encoding="utf-8") as config:
                config.write(text.replace(BUILD_IP, host_ip))
            target.upload(local, config_path(app))


def daemon_ready(target) -> bool:
    """Return True if the daemon has made a local socket."""
    return target.execute(f'set -- {SOCKET_GLOB}; test -S "$1"')[0] == 0


def launch_daemon(target, app: str):
    """Start someip-daemon with the config of *app* and wait for its socket.

    :raises RuntimeError: the daemon did not make its socket. Its output above the failure gives the cause.
    """
    target.execute("rm -rf /tmp/someip")
    process = target.execute_async(f"{ROOT}/bin/someip-daemon", [config_path(app)])
    deadline = time.monotonic() + 10.0
    while not daemon_ready(target):
        if time.monotonic() > deadline or not process.is_running():
            raise RuntimeError(f"someip-daemon did not create {SOCKET_GLOB}; see its streamed output above.")
        time.sleep(0.2)
    return process


def launch_app(target, app: str):
    """Start the *app* test application (``ets`` or ``tc8``) on its config."""
    return target.execute_async(f"{ROOT}/bin/{app}_app", [config_path(app)])


def relaunch_dut(target, config=None, *, settle: bool = True):
    """Kill the ETS app in *target*, start a new one, and return its handle.

    The handle is also in ``target.ets_process``.
    """
    kill_dut(target, missing_ok=True)
    process = launch_app(target, ETS)
    if settle:
        time.sleep(0.5)
        if not process.is_running():
            raise RuntimeError("ets_app exited right after launch; see logs above.")
    target.ets_process = process
    return process


@contextlib.contextmanager
def ets_stack(target, root=None, config=None):
    """Run the daemon and the ETS app on *target*, and yield the target.

    The app handle is ``target.ets_process``. The daemon handle is ``target.someipd_process``.
    """
    daemon = launch_daemon(target, ETS)
    relaunch_dut(target)
    target.someipd_process = daemon
    try:
        yield target
    finally:
        for name, process in (("ets_app", target.ets_process), ("someip-daemon", daemon)):
            try:
                process.stop()
            except Exception:  # noqa: BLE001
                logger.warning("stopping %s raised during teardown", name, exc_info=True)


def kill_dut(target, *, missing_ok: bool = False) -> None:
    """Send SIGKILL to the ETS app only. The daemon continues to run, so a client sees the offer TTL expire.

    The kill uses the PID, because the integration-test image has no pkill.
    """
    process = getattr(target, "ets_process", None)
    if process is None or not process.is_running():
        if missing_ok:
            return
        raise RuntimeError("no ETS app is running on the target, so nothing was killed")
    target.execute(f"kill -9 {process.pid()}")
    deadline = time.monotonic() + 5.0
    while process.is_running():
        if time.monotonic() > deadline:
            raise RuntimeError("ets_app still running 5s after SIGKILL")
        time.sleep(0.05)
