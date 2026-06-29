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
"""Pytest plugin that boots **two** QNX QEMU VMs sharing a QEMU ``ivshmem`` region.

It is a thin extension of the single-VM ``qemu`` plugin
(``@score_itf//score/itf/plugins/qemu``): it reuses ``QemuTarget`` / ``pre_tests_phase``
and only adds (a) a second VM, (b) an ``ivshmem-plain`` device backed by one shared host
file, and (c) distinct host SSH ports per VM.

Exposed session fixtures:
    - ``target_a`` / ``target_b`` -- the two booted VMs (``QemuTarget``).
    - ``ivshmem_backend``         -- path of the shared host backing file.
"""
import logging
import socket

import pytest

from score.itf.core.utils.bunch import Bunch

from .config import load_configuration, parse_size
from .dual_qemu_process import DualQemuProcess

logger = logging.getLogger(__name__)


def pytest_addoption(parser):
    parser.addoption(
        "--dual-qemu-config",
        action="store",
        required=True,
        help="Path to a JSON file describing the two VMs and the ivshmem region.",
    )
    parser.addoption(
        "--qemu-image-a",
        action="store",
        required=True,
        help="Path to the QEMU IFS image for VM-A.",
    )
    parser.addoption(
        "--qemu-image-b",
        action="store",
        required=True,
        help="Path to the QEMU IFS image for VM-B.",
    )


@pytest.fixture(scope="session")
def config(request):
    return Bunch(
        dual_config=load_configuration(request.config.getoption("dual_qemu_config")),
        qemu_images=[
            request.config.getoption("qemu_image_a"),
            request.config.getoption("qemu_image_b"),
        ],
    )


@pytest.fixture(scope="session")
def ivshmem_backend(config, tmp_path_factory):
    """Create the single host backing file both VMs map as their ivshmem region."""
    ivshmem = config.dual_config.ivshmem
    size_bytes = parse_size(ivshmem.size)

    if ivshmem.mem_path:
        path = ivshmem.mem_path
    else:
        path = str(tmp_path_factory.mktemp("ivshmem") / "score_ivshmem")

    logger.info(f"Creating ivshmem backing file {path} ({ivshmem.size})")
    with open(path, "wb") as backing_file:
        backing_file.truncate(size_bytes)

    yield path


@pytest.fixture(scope="session")
def _targets(config, ivshmem_backend):
    """Boot both VMs sequentially, verify them, and tear down in reverse order."""
    logger.info(f"Starting dual-VM tests on host: {socket.gethostname()}")
    dual_config = config.dual_config

    # When the inter-VM network is enabled, VM-A hosts the socket and VM-B connects.
    intervm = dual_config.intervm_network
    intervm_roles = [None, None]
    if intervm.enabled:
        intervm_roles = [("listen", intervm.host_port), ("connect", intervm.host_port)]

    vms = dual_config.vms

    # Boot VM-A first, then VM-B. Sequential booting avoids a KVM race where two QNX
    # guests initializing concurrently can wedge the second guest's device bring-up.
    with DualQemuProcess(
        config.qemu_images[0],
        vms[0].qemu_ram_size,
        vms[0].qemu_num_cores,
        vm_config=vms[0],
        port_forwarding=vms[0].port_forwarding,
        ivshmem_path=ivshmem_backend,
        ivshmem_size=dual_config.ivshmem.size,
        intervm=intervm_roles[0],
        vm_index=0,
    ) as process_a:
        with DualQemuProcess(
            config.qemu_images[1],
            vms[1].qemu_ram_size,
            vms[1].qemu_num_cores,
            vm_config=vms[1],
            port_forwarding=vms[1].port_forwarding,
            ivshmem_path=ivshmem_backend,
            ivshmem_size=dual_config.ivshmem.size,
            intervm=intervm_roles[1],
            vm_index=1,
        ) as process_b:
            # Re-verify VM-A is still responsive (it may have gone quiet while VM-B booted).
            process_a.ensure_responsive()
            yield [process_a.target, process_b.target]


@pytest.fixture(scope="session")
def target_a(_targets):
    """The first VM (VM-A) in the inter-VM shared-memory tests."""
    return _targets[0]


@pytest.fixture(scope="session")
def target_b(_targets):
    """The second VM (VM-B) in the inter-VM shared-memory tests."""
    return _targets[1]
