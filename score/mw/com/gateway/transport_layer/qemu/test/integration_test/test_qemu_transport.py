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
"""

import logging
import time

logger = logging.getLogger(__name__)

APP1 = "/opt/qemu_transport_test/bin/app1"
APP2 = "/opt/qemu_transport_test/bin/app2"
VM_A_LABEL = "VM-A (src)"
VM_B_LABEL = "VM-B (dest)"
TIMEOUT_SECONDS = 90


def _collect_result(label, process):
    rc = process.get_exit_code()
    text = process.get_output().strip()
    logger.info("==================== %s ====================", label)
    logger.info("%s (rc=%s)", text, rc)
    print(f"\n[{label}] {text} (rc={rc})")
    return rc, text


def test_qemu_ivshmem_transport(target_a, target_b):
    """Bidirectional: VM-A writes service_a and reads service_b; VM-B writes service_b and reads service_a."""
    processes = {
        VM_A_LABEL: target_a.execute_async(APP1),
        VM_B_LABEL: target_b.execute_async(APP2),
    }
    results = {}
    deadline = time.monotonic() + TIMEOUT_SECONDS
    while processes and time.monotonic() < deadline:
        for label, process in list(processes.items()):
            if not process.is_running():
                results[label] = _collect_result(label, process)
                del processes[label]
                if results[label][0] != 0:
                    for peer_process in processes.values():
                        peer_process.stop()
                    break
        time.sleep(0.1)

    if processes:
        for process in processes.values():
            process.stop()
        raise TimeoutError(f"Timed out after {TIMEOUT_SECONDS}s waiting for: {list(processes)}")

    rc_a, text_a = results[VM_A_LABEL]
    rc_b, text_b = results[VM_B_LABEL]

    assert rc_a == 0, f"source (VM-A) failed (rc={rc_a}): {text_a}"
    assert rc_b == 0, f"destination (VM-B) failed (rc={rc_b}): {text_b}"
    assert "verified" in text_a, f"source did not verify: {text_a!r}"
    assert "verified" in text_b, f"destination did not verify: {text_b!r}"
