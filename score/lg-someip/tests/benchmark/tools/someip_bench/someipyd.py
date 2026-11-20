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

"""The someipy daemon for the host-side peer scripts. Each script starts one, bound to the peer address."""

from __future__ import annotations

import asyncio
import json
import logging
import os
import sys
import tempfile
from pathlib import Path

from someipy import connect_to_someipy_daemon


async def start_daemon(
    *, sd_address: str, sd_port: int, interface: str, socket_path: str, log_level: str, script: Path | None = None
):
    """Start someipyd, or the patched daemon *script*, and connect to it when its socket appears.

    Returns ``(client, process)``. If the socket does not appear in 10 s, stop the daemon and return ``None``.
    """
    fd, cfg_path = tempfile.mkstemp(prefix="someipyd_", suffix=".json")
    with os.fdopen(fd, "w") as f:
        json.dump(
            {
                "sd_address": sd_address,
                "sd_port": sd_port,
                "interface": interface,
                "socket_path": socket_path,
                "log_level": log_level,
            },
            f,
        )
    daemon_argv = [script] if script is not None else ["-m", "someipy.someipyd"]
    process = await asyncio.create_subprocess_exec(sys.executable, *daemon_argv, "--config", cfg_path)
    for _ in range(100):
        if Path(socket_path).exists():
            # The daemon reads its configuration only one time, at startup.
            os.unlink(cfg_path)
            return await connect_to_someipy_daemon({"socket_path": socket_path}), process
        await asyncio.sleep(0.1)

    process.terminate()
    os.unlink(cfg_path)
    return None


async def stop_daemon(client, process) -> None:
    """Disconnect *client* and stop the daemon *process*. Kill it if it does not stop in 5 s."""
    # Silence the client logger for the disconnect. It shows the closed daemon socket as an error.
    # Without this, that error is the last line in the log of a successful run.
    logging.getLogger("someipy.someipyd_client").setLevel(logging.CRITICAL)
    await client.disconnect_from_daemon()
    process.terminate()
    try:
        await asyncio.wait_for(process.wait(), timeout=5)
    except asyncio.TimeoutError:
        process.kill()
