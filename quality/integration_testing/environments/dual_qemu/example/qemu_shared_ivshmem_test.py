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
"""Cross-VM shared-memory DATA-PLANE example (driven by a C++ binary).

This exercises the property that makes the LoLa data plane work across VMs: a pointer
stored INSIDE the shared ivshmem region must resolve on the other VM even though each VM
maps the region at a different virtual base address. The C++ program (``qemu_shared_ivshmem.cpp``)
stores a singly-linked list whose links are self-relative byte offsets ("offset pointers",
like ``score::memory::shared::OffsetPtr``) instead of raw pointers.

Each VM does BOTH write and read in a SINGLE run:

  * VM-A (``--id A``) writes an offset-pointer list into its slot, then reads + verifies
    VM-B's list (slot B).
  * VM-B (``--id B``) writes an offset-pointer list into its slot, then reads + verifies
    VM-A's list (slot A).

Because each run waits for the peer, the two commands are launched CONCURRENTLY so the VMs
rendezvous. Each run prints the base address it mapped the region at, so the logs show the
two VMs resolving the same structures from different bases.

Run it and watch BOTH VMs' results with::

    bazel test //quality/integration_testing/environments/dual_qemu/example:test_qemu_shared_ivshmem \\
        --config=qnx_x86_64 --test_output=streamed

``--test_output=streamed`` (or ``=all``) is what makes the ``[VM-A ...]`` and
``[VM-B ...]`` lines visible on your console.
"""
import logging
import threading

logger = logging.getLogger(__name__)

APP = "bin/qemu_shared_ivshmem"
CWD = "/opt/qemu_shared_ivshmem"


def _run(target, label, args, results):
    rc, out = target.execute(f"cd {CWD} && {APP} {args}")
    text = out.decode(errors="replace").strip()
    logger.info("==================== %s ====================", label)
    logger.info("%s (rc=%s)", text, rc)
    print(f"\n[{label}] {text} (rc={rc})")
    results[label] = (rc, text)


def test_simple_shared_memory_read_write(target_a, target_b):
    """Each VM writes its own offset-pointer list and reads+verifies the peer's, in one run.

    The two single-run commands are launched concurrently so the VMs can rendezvous (each
    run waits for the peer to publish). The binary reaches the shared ivshmem region through
    pci-server, so no /dev/ivshmem0 guest driver is required.
    """
    results = {}
    threads = [
        threading.Thread(target=_run, args=(target_a, "VM-A", "--id A", results)),
        threading.Thread(target=_run, args=(target_b, "VM-B", "--id B", results)),
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    rc_a, text_a = results["VM-A"]
    rc_b, text_b = results["VM-B"]

    # Both runs must succeed: the offset pointers written by one VM resolved correctly on
    # the other VM despite the different mapping base.
    assert rc_a == 0, f"VM-A run failed (rc={rc_a}): {text_a}"
    assert rc_b == 0, f"VM-B run failed (rc={rc_b}): {text_b}"
    assert "verified" in text_a, f"VM-A did not verify the peer list, got: {text_a!r}"
    assert "verified" in text_b, f"VM-B did not verify the peer list, got: {text_b!r}"
