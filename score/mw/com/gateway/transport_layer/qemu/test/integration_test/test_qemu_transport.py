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
"""Bidirectional integration test for the QEMU ivshmem gateway transport layer.

Both VMs run QemuHypervisorTransport simultaneously as source and destination:
  - VM-A creates shm for service_a (payload [0xCAFEBABE, 100, 200]) → VM-B reads
  - VM-B creates shm for service_b (payload [0xDEADBEEF, 300, 400]) → VM-A reads

Shared memory (the ivshmem BAR) carries only the data plane; cross-VM synchronization
(data-ready / verified signaling) travels over the real socket-based BidirectionalTransport
via QemuHypervisorTransport::NotifyUpdate, matching ivshmem-plain's lack of a doorbell/MSI-X.
Both apps print "verified" on success.

Diagnostics: interrupt report from the guests and a qmp snapshot per VM before the apps start,
then every 10s if the run takes too long, and once more on timeout. A hang should explain
itself in the log.
"""

import logging
import subprocess
import threading
from concurrent.futures import ThreadPoolExecutor

logger = logging.getLogger(__name__)

APP1 = "/opt/qemu_transport_test/bin/app1"
APP2 = "/opt/qemu_transport_test/bin/app2"
VM_A_LABEL = "VM-A (src)"
VM_B_LABEL = "VM-B (dest)"
TIMEOUT_SECONDS = 180
# healthy runs take a few seconds
WATCHDOG_GRACE_S = 20
WATCHDOG_INTERVAL_S = 10


def _run_app(label, console, app):
    """Run one app over the serial console and return rc + output. No ssh here on purpose."""
    exit_code, output = console.run_sh_cmd_output(app, timeout=TIMEOUT_SECONDS)
    text = output.strip()
    logger.info("==================== %s ====================", label)
    logger.info("%s (rc=%s)", text, exit_code)
    return exit_code, text


def _host_socket_state(port):
    """ss output for the socket linking the two qemus."""
    if port is None:
        return "intervm socket: disabled"
    try:
        result = subprocess.run(
            ["ss", "-tni", f"( sport = :{port} or dport = :{port} )"],
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )
        return f"intervm host socket (port {port}):\n{result.stdout.strip() or result.stderr.strip()}"
    except Exception as ex:  # pylint: disable=broad-except
        return f"intervm host socket (port {port}): ss failed: {ex}"


def _snapshot(vm_a, vm_b, port, reason):
    logger.warning(
        "DIAGNOSTIC SNAPSHOT (%s)\n%s\n%s\n%s", reason, vm_a.diagnose(), vm_b.diagnose(), _host_socket_state(port)
    )


def _watchdog(vm_a, vm_b, port, stop_event):
    if stop_event.wait(WATCHDOG_GRACE_S):
        return
    sample = 0
    while not stop_event.is_set():
        sample += 1
        _snapshot(vm_a, vm_b, port, f"still running after {WATCHDOG_GRACE_S + (sample - 1) * WATCHDOG_INTERVAL_S}s")
        stop_event.wait(WATCHDOG_INTERVAL_S)


def test_qemu_ivshmem_transport(console_a, console_b, vm_a, vm_b, intervm_host_port):
    """Bidirectional: VM-A writes service_a and reads service_b; VM-B mirrors it."""
    # baseline while the consoles are still free
    logger.info("%s\n%s", vm_a.guest_interrupt_report(), vm_b.guest_interrupt_report())
    _snapshot(vm_a, vm_b, intervm_host_port, "baseline before application start")

    stop_watchdog = threading.Event()
    watchdog = threading.Thread(target=_watchdog, args=(vm_a, vm_b, intervm_host_port, stop_watchdog), daemon=True)
    watchdog.start()
    try:
        # the apps wait for each other, so run both at once
        with ThreadPoolExecutor(max_workers=2) as executor:
            future_a = executor.submit(_run_app, VM_A_LABEL, console_a, APP1)
            future_b = executor.submit(_run_app, VM_B_LABEL, console_b, APP2)
            try:
                rc_a, text_a = future_a.result(timeout=TIMEOUT_SECONDS + 30)
                rc_b, text_b = future_b.result(timeout=TIMEOUT_SECONDS + 30)
            except Exception:
                _snapshot(vm_a, vm_b, intervm_host_port, "post-mortem at failure")
                raise
    finally:
        stop_watchdog.set()
        watchdog.join(timeout=5)

    assert rc_a == 0, f"source (VM-A) failed (rc={rc_a}): {text_a}"
    assert rc_b == 0, f"destination (VM-B) failed (rc={rc_b}): {text_b}"
    assert "verified" in text_a, f"source did not verify: {text_a!r}"
    assert "verified" in text_b, f"destination did not verify: {text_b!r}"
