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
"""Stage-2 CROSS-VM test: PRODUCTION lib memory over the ivshmem BAR (Create + Open).

VM-A runs ``--role producer`` (SharedMemoryFactory::Create + init an OffsetPtr list on the
BAR); VM-B runs ``--role consumer`` (binds its own name to the same BAR, SharedMemoryFactory::
Open, then resolves + verifies the producer's OffsetPtr graph). The two rendezvous through a
handshake header in the last page of the BAR, so they must run CONCURRENTLY.

This proves the second VM can attach to the same physical BAR through unmodified lib memory
and resolve the producer's OffsetPtr graph despite mapping the BAR at a different base.

    bazel test //quality/integration_testing/environments/dual_qemu/example:test_lib_memory_ivshmem_xvm \\
        --config=qnx_x86_64 --test_output=streamed
"""
import logging
import threading

logger = logging.getLogger(__name__)

APP = "bin/lib_memory_ivshmem_xvm"
CWD = "/opt/lib_memory_ivshmem_xvm"


def _run(target, label, role, results):
    rc, out = target.execute(f"cd {CWD} && {APP} --role {role}")
    text = out.decode(errors="replace").strip()
    logger.info("==================== %s (%s) ====================", label, role)
    logger.info("%s (rc=%s)", text, rc)
    print(f"\n[{label}] {text} (rc={rc})")
    results[label] = (rc, text)


def test_cross_vm_lib_memory_over_bar(target_a, target_b):
    """Producer Creates the region on the BAR; consumer Opens it and resolves the OffsetPtr list."""
    results = {}
    threads = [
        threading.Thread(target=_run, args=(target_a, "VM-A", "producer", results)),
        threading.Thread(target=_run, args=(target_b, "VM-B", "consumer", results)),
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    rc_a, text_a = results["VM-A"]
    rc_b, text_b = results["VM-B"]

    assert rc_a == 0, f"producer (VM-A) failed (rc={rc_a}): {text_a}"
    assert rc_b == 0, f"consumer (VM-B) failed (rc={rc_b}): {text_b}"
    assert "verified" in text_a, f"producer did not complete the handshake, got: {text_a!r}"
    assert "verified" in text_b, f"consumer did not verify the list, got: {text_b!r}"
