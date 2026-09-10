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

import concurrent.futures
import logging
import socket

import pytest

from score.itf.core.utils.bunch import Bunch

from .config import load_configuration, parse_size
from .dual_qemu_process import DualQemuProcess

logger = logging.getLogger(__name__)


def _free_tcp_ports(count: int) -> list[int]:
    """Reserve ``count`` distinct free loopback ports.

    Fixed host ports are unsafe here. VM-B's inter-VM netdev uses ``reconnect=1``, so a QEMU
    still winding down from a previous run keeps dialling once a second and can grab the next
    run's VM-A listener before that run's own VM-B boots; VM-A accepts one connection, so both
    guests then look connected while no traffic crosses. Fixed SSH ports collide the same way
    between concurrent runs. All probes are held open together so the ports differ.
    """
    probes = []
    try:
        for _ in range(count):
            probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            probe.bind(("127.0.0.1", 0))
            probes.append(probe)
        return [probe.getsockname()[1] for probe in probes]
    finally:
        for probe in probes:
            probe.close()


def _assign_ssh_port(vm_config, port: int) -> None:
    """Move this VM's SSH host port, keeping the guest-side forwarding entry in step."""
    for forwarding in vm_config.port_forwarding:
        if forwarding.host_port == vm_config.ssh_port:
            forwarding.host_port = port
    vm_config.ssh_port = port


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
    vms = dual_config.vms
    ports = iter(_free_tcp_ports(len(vms) + 1))

    # When the inter-VM network is enabled, VM-A hosts the socket and VM-B connects.
    intervm = dual_config.intervm_network
    intervm_roles = [None, None]
    if intervm.enabled:
        host_port = intervm.host_port or next(ports)
        logger.info(f"Inter-VM link on host port {host_port}")
        intervm_roles = [("listen", host_port), ("connect", host_port)]

    if dual_config.auto_ssh_ports:
        for vm in vms:
            _assign_ssh_port(vm, next(ports))
        logger.info(f"SSH host ports: {[vm.ssh_port for vm in vms]}")

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
            # Re-verify both VMs are still responsive (either may have gone quiet while the other
            # booted). The probes are read-only SSH checks on already-booted VMs, so running them
            # concurrently is safe; any restart needed to self-heal still runs sequentially below
            # to avoid the concurrent-boot wedge that sequential *booting* was designed to avoid.
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                healthy_a, healthy_b = pool.map(lambda p: p.is_responsive(), [process_a, process_b])
            if not healthy_a:
                process_a.self_heal()
            if not healthy_b:
                process_b.self_heal()
            yield [process_a.target, process_b.target]


@pytest.fixture(scope="session")
def target_a(_targets):
    """The first VM (VM-A) in the inter-VM shared-memory tests."""
    return _targets[0]


@pytest.fixture(scope="session")
def target_b(_targets):
    """The second VM (VM-B) in the inter-VM shared-memory tests."""
    return _targets[1]
