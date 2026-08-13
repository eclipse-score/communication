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

The handshake uses the last page of the ivshmem BAR with a phased protocol.
Both apps print "verified" on success.
"""
import logging
import threading
import time

logger = logging.getLogger(__name__)

APP1 = "/opt/qemu_transport_test/bin/app1"
APP2 = "/opt/qemu_transport_test/bin/app2"
VM_A_LABEL = "VM-A (src)"
VM_B_LABEL = "VM-B (dest)"
TIMEOUT_SECONDS = 180


def _run(target, label, app_path, results):
    rc, out = target.execute(app_path)
    text = out.decode(errors="replace").strip()
    logger.info("==================== %s ====================", label)
    logger.info("%s (rc=%s)", text, rc)
    print(f"\n[{label}] {text} (rc={rc})")
    results[label] = (rc, text)


def test_qemu_ivshmem_transport(target_a, target_b):
    """Bidirectional: VM-A writes service_a and reads service_b; VM-B writes service_b and reads service_a."""
    results = {}
    threads = [
        threading.Thread(target=_run, args=(target_a, VM_A_LABEL, APP1, results), name=VM_A_LABEL),
        threading.Thread(target=_run, args=(target_b, VM_B_LABEL, APP2, results), name=VM_B_LABEL),
    ]
    for t in threads:
        t.start()
    deadline = time.monotonic() + TIMEOUT_SECONDS
    for t in threads:
        remaining = deadline - time.monotonic()
        if remaining > 0:
            t.join(timeout=remaining)
    alive_threads = [t.name for t in threads if t.is_alive()]
    if alive_threads:
        raise TimeoutError(f"Timed out after {TIMEOUT_SECONDS}s waiting for: {alive_threads}")

    missing_results = [label for label in [VM_A_LABEL, VM_B_LABEL] if label not in results]
    if missing_results:
        raise AssertionError(f"Missing test results for: {missing_results}")

    rc_a, text_a = results[VM_A_LABEL]
    rc_b, text_b = results[VM_B_LABEL]

    assert rc_a == 0, f"source (VM-A) failed (rc={rc_a}): {text_a}"
    assert rc_b == 0, f"destination (VM-B) failed (rc={rc_b}): {text_b}"
    assert "verified" in text_a, f"source did not verify: {text_a!r}"
    assert "verified" in text_b, f"destination did not verify: {text_b!r}"
