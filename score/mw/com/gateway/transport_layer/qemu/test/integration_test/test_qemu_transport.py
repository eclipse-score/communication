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
via QemuHypervisorTransport::NotifyUpdate. Both apps print "verified" on success.
"""

import logging
import re
from concurrent.futures import ThreadPoolExecutor

logger = logging.getLogger(__name__)

APP1 = "/opt/qemu_transport_test/bin/app1"
APP2 = "/opt/qemu_transport_test/bin/app2"
VM_A_LABEL = "VM-A (src)"
VM_B_LABEL = "VM-B (dest)"
TIMEOUT_SECONDS = 180
VERIFIED_LINE = re.compile(r"(?m)^verified$")


def _run(console, label, app_path):
    rc, out = console.run_sh_cmd_output(app_path, timeout=TIMEOUT_SECONDS)
    text = out.strip()
    logger.info("==================== %s ====================", label)
    logger.info("%s (rc=%s)", text, rc)
    print(f"\n[{label}] {text} (rc={rc})")
    return rc, text


def test_qemu_ivshmem_transport(console_a, console_b):
    """Bidirectional: VM-A writes service_a and reads service_b; VM-B writes service_b and reads service_a."""
    # Both applications wait for messages from the other VM and must start together.
    with ThreadPoolExecutor(max_workers=2) as executor:
        future_a = executor.submit(_run, console_a, VM_A_LABEL, APP1)
        future_b = executor.submit(_run, console_b, VM_B_LABEL, APP2)
        try:
            rc_a, text_a = future_a.result(timeout=TIMEOUT_SECONDS)
            rc_b, text_b = future_b.result(timeout=TIMEOUT_SECONDS)
        except Exception:
            logger.exception("QEMU transport application execution failed")
            raise

    assert rc_a == 0, f"source (VM-A) failed (rc={rc_a}): {text_a}"
    assert rc_b == 0, f"destination (VM-B) failed (rc={rc_b}): {text_b}"
    assert VERIFIED_LINE.search(text_a), f"source did not verify: {text_a!r}"
    assert VERIFIED_LINE.search(text_b), f"destination did not verify: {text_b!r}"
