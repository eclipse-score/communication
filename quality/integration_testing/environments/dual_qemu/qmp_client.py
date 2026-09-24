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
"""Small QMP client, only for diagnostics.

Gives the host view of a guest while a test hangs: info network (is the intervm socket
connected), info pci (which IRQ line the NICs are on), info pic (IOAPIC table as the guest
programmed it, plus the live IRR), and the virtio isr + ring indices (frames delivered vs
consumed). Note: isr only means something on INTx, with MSI-X the driver never reads it.
"""

import json
import logging
import re
import socket
import threading

logger = logging.getLogger(__name__)

# qemu takes one qmp client per socket at a time
_socket_locks = {}
_socket_locks_guard = threading.Lock()


def _lock_for(sock_path):
    with _socket_locks_guard:
        return _socket_locks.setdefault(sock_path, threading.Lock())


class QmpClient:
    """Synchronous QMP over a unix socket."""

    def __init__(self, sock_path: str, timeout: float = 5.0):
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.settimeout(timeout)
        self._sock.connect(sock_path)
        self._buffer = b""
        self._recv_json()  # server greeting ({"QMP": {...}})
        self.qmp("qmp_capabilities")

    def _send(self, obj):
        self._sock.sendall((json.dumps(obj) + "\n").encode())

    def _recv_json(self):
        while b"\n" not in self._buffer:
            chunk = self._sock.recv(65536)
            if not chunk:
                raise ConnectionError("QMP socket closed unexpectedly")
            self._buffer += chunk
        line, _, self._buffer = self._buffer.partition(b"\n")
        return json.loads(line.decode())

    def qmp(self, command: str, arguments=None):
        """Run one QMP command, return its payload, raise on error."""
        message = {"execute": command}
        if arguments:
            message["arguments"] = arguments
        self._send(message)
        while True:
            response = self._recv_json()
            if "event" in response:  # asynchronous events interleave with replies
                continue
            if "error" in response:
                raise RuntimeError(f"{command}: {response['error']}")
            return response.get("return")

    def hmp(self, command_line: str) -> str:
        """Run an HMP command, return the text."""
        return self.qmp("human-monitor-command", {"command-line": command_line})

    def info_network(self) -> str:
        return self.hmp("info network")

    def close(self):
        try:
            self._sock.close()
        except OSError:
            pass


def try_info_network(sock_path: str, label: str) -> str:
    """info network, never raises."""
    try:
        with _lock_for(sock_path):
            client = QmpClient(sock_path)
            try:
                return f"{label}: {client.info_network()}"
            finally:
                client.close()
    except Exception as ex:  # pylint: disable=broad-except
        return f"{label}: QMP query failed: {ex}"


def _pci_summary(info_pci: str) -> str:
    """One line per PCI function with its IRQ line."""
    lines = []
    current = None
    for line in info_pci.splitlines():
        header = re.match(r"\s*Bus\s+(\d+), device\s+(\d+), function (\d+):", line)
        if header:
            current = f"{int(header.group(1)):02x}:{int(header.group(2)):02x}.{header.group(3)}"
            continue
        if current is None:
            continue
        cls = re.match(r"\s{4}(\S.*?): PCI device (\S+)", line)
        if cls:
            lines.append(f"    {current} {cls.group(2)} {cls.group(1)}")
            continue
        irq = re.match(r"\s+IRQ (\d+), pin (\S)", line)
        if irq and lines:
            lines[-1] += f"  IRQ {irq.group(1)} pin {irq.group(2)}"
    return "\n".join(lines)


def _ioapic_summary(info_pic: str, pins=(9, 10, 11)) -> str:
    """IOAPIC pins 9-11 and the IRR words."""
    keep = []
    for line in info_pic.splitlines():
        stripped = line.strip()
        if stripped.startswith("ioapic") or stripped.startswith("IRR") or stripped.startswith("Remote IRR"):
            keep.append(f"    {stripped}")
            continue
        pin = re.match(r"\s*pin (\d+)\s", line)
        if pin and int(pin.group(1)) in pins:
            keep.append(f"    {stripped}")
    return "\n".join(keep)


def _virtio_net_summary(client: QmpClient) -> str:
    """isr, ring features and rx/tx ring indices per virtio-net device."""
    out = []
    for dev in client.qmp("x-query-virtio"):
        if "virtio-net" not in dev.get("name", ""):
            continue
        path = dev["path"]
        status = client.qmp("x-query-virtio-status", {"path": path})
        ring_features = [
            feat.split(":")[0]
            for feat in status.get("guest-features", {}).get("transports", [])
            if "EVENT_IDX" in feat or "INDIRECT" in feat or "VERSION_1" in feat or "NOTIFY_ON_EMPTY" in feat
        ]
        out.append(
            f"    {path}: isr={status.get('isr')} status={[s.split(':')[0] for s in status.get('status', [])]} "
            f"ring_features={ring_features}"
        )
        for queue, role in ((0, "rx"), (1, "tx")):
            q = client.qmp("x-query-virtio-queue-status", {"path": path, "queue": queue})
            out.append(
                f"      vq{queue} ({role}): num={q.get('vring-num')} inuse={q.get('inuse')} "
                f"last_avail={q.get('last-avail-idx')} shadow_avail={q.get('shadow-avail-idx')} "
                f"used={q.get('used-idx')} signalled_used={q.get('signalled-used')} "
                f"(valid={q.get('signalled-used-valid')})"
            )
    return "\n".join(out)


def diagnose(sock_path: str, label: str) -> str:
    """Snapshot of one VM, never raises, failed queries are reported inline."""
    parts = [f"{label} host-side state:"]
    try:
        with _lock_for(sock_path):
            client = QmpClient(sock_path)
            try:
                for title, func in (
                    ("netdevs", lambda: "    " + client.info_network().replace("\n", "\n    ")),
                    ("pci", lambda: _pci_summary(client.hmp("info pci"))),
                    ("ioapic", lambda: _ioapic_summary(client.hmp("info pic"))),
                    ("virtio-net", lambda: _virtio_net_summary(client)),
                ):
                    try:
                        parts.append(f"  {title}:\n{func()}")
                    except Exception as ex:  # pylint: disable=broad-except
                        parts.append(f"  {title}: query failed: {ex}")
            finally:
                client.close()
    except Exception as ex:  # pylint: disable=broad-except
        parts.append(f"  QMP connect failed: {ex}")
    return "\n".join(parts)
