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
# Must exceed HyperVisorSocketConfiguration::setup_timeout_ms_ (90s, see app1_main.cpp) plus
# margin for the protocol exchange itself.
TIMEOUT_SECONDS = 150


def _collect_result(label, process):
    rc = process.get_exit_code()
    text = process.get_output().strip()
    logger.info("==================== %s ====================", label)
    logger.info("%s (rc=%s)", text, rc)
    print(f"\n[{label}] {text} (rc={rc})")
    return rc, text


def _stop_targets(target_a, target_b):
    """Stop the local QEMU processes without requiring guest SSH connectivity."""
    for target in [target_a, target_b]:
        try:
            target.kill_process()
        except Exception:  # pylint: disable=broad-except
            logger.exception("Failed to stop local QEMU process")


def _collect_stopped_results(processes, results):
    for label, process in processes.items():
        try:
            process.wait(timeout_s=5)
        except Exception:  # pylint: disable=broad-except
            logger.exception("Failed to collect stopped process %s", label)
        results[label] = _collect_result(label, process)


def _launch_with_retry(target, binary, label, grace_period_s=20, restart_attempts=2):
    """Launches `binary` via SSH, giving sshd a grace period before restarting the VM.

    A VM's sshd can go idle-dead at any point (not just during fixture setup, see
    qnx-qemu-networking notes). A short blip often clears on its own, so retry once after a
    grace wait before resorting to target.restart() (a full reboot + pre_tests_phase), which
    re-runs DualQemuProcess's self-healing boot.
    """
    try:
        return target.execute_async(binary)
    except Exception as ex:  # pylint: disable=broad-except
        last_error = ex
        logger.warning("%s: execute_async failed: %s; waiting %ds before retrying", label, ex, grace_period_s)
        time.sleep(grace_period_s)

    for attempt in range(1, restart_attempts + 1):
        try:
            return target.execute_async(binary)
        except Exception as ex:  # pylint: disable=broad-except
            last_error = ex
            logger.warning(
                "%s: still unreachable (attempt %d/%d): %s; restarting VM", label, attempt, restart_attempts, ex
            )
            target.restart()
    raise last_error


def test_qemu_ivshmem_transport(target_a, target_b):
    """Bidirectional: VM-A writes service_a and reads service_b; VM-B writes service_b and reads service_a."""
    processes = {
        VM_A_LABEL: _launch_with_retry(target_a, APP1, VM_A_LABEL),
        VM_B_LABEL: _launch_with_retry(target_b, APP2, VM_B_LABEL),
    }
    results = {}
    deadline = time.monotonic() + TIMEOUT_SECONDS
    while processes and time.monotonic() < deadline:
        for label, process in list(processes.items()):
            if not process.is_running():
                results[label] = _collect_result(label, process)
                del processes[label]
                if results[label][0] != 0:
                    _stop_targets(target_a, target_b)
                    _collect_stopped_results(processes, results)
                    processes.clear()
                    break
        time.sleep(0.1)

    if processes:
        remaining_labels = list(processes)
        _stop_targets(target_a, target_b)
        _collect_stopped_results(processes, results)
        processes.clear()
        raise TimeoutError(f"Timed out after {TIMEOUT_SECONDS}s waiting for: {remaining_labels}")

    rc_a, text_a = results[VM_A_LABEL]
    rc_b, text_b = results[VM_B_LABEL]

    assert rc_a == 0, f"source (VM-A) failed (rc={rc_a}): {text_a}"
    assert rc_b == 0, f"destination (VM-B) failed (rc={rc_b}): {text_b}"
    assert "verified" in text_a, f"source did not verify: {text_a!r}"
    assert "verified" in text_b, f"destination did not verify: {text_b!r}"
