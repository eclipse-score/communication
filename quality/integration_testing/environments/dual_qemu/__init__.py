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
(``@score_itf//score/itf/plugins/qemu``): it reuses ``QemuTarget``
and only adds (a) a second VM, (b) an ``ivshmem-plain`` device backed by one shared host
file, and (c) distinct host SSH ports per VM.

Exposed session fixtures:
    - ``target_a`` / ``target_b``   -- the two booted VMs (``QemuTarget``).
    - ``console_a`` / ``console_b`` -- their serial consoles.
    - ``vm_a`` / ``vm_b``           -- the ``DualQemuProcess`` objects (diagnostics).
    - ``intervm_host_port``         -- host port of the socket linking the two VMs.
    - ``ivshmem_backend``           -- path of the shared host backing file.
"""

import logging
import os
import socket

import pytest

from score.itf.core.utils.bunch import Bunch

from .config import load_configuration, parse_size
from .dual_qemu_process import (
    DualQemuProcess,
    allocate_free_host_port,
    execute_async_with_retries,
    require_free_host_ports,
    stop_quietly,
    wait_for_host_port_bound,
)

# Helpers used by tests that launch applications over SSH.
__all__ = ["execute_async_with_retries", "stop_quietly"]

logger = logging.getLogger(__name__)

# what the apps connect to
_INTERVM_ADDRESSES = ("10.0.3.1", "10.0.3.2")


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


def _diagnostics_dir(tmp_path_factory):
    """pcaps go to bazel undeclared outputs when available."""
    undeclared = os.environ.get("TEST_UNDECLARED_OUTPUTS_DIR")
    if undeclared and os.path.isdir(undeclared):
        return undeclared
    return str(tmp_path_factory.mktemp("netdump"))


@pytest.fixture(scope="session")
def _targets(config, ivshmem_backend, tmp_path_factory):
    """Boot both VMs sequentially, verify them, and tear down in reverse order."""
    logger.info(f"Starting dual-VM tests on host: {socket.gethostname()}")
    dual_config = config.dual_config
    vms = dual_config.vms
    intervm = dual_config.intervm_network

    # diagnostics only, guests don't see any of this
    qmp_dir = tmp_path_factory.mktemp("qmp")
    qmp_socket_paths = [str(qmp_dir / "vm_a.qmp"), str(qmp_dir / "vm_b.qmp")]
    dump_dir = _diagnostics_dir(tmp_path_factory)
    logger.info("Netdev pcaps go to %s", dump_dir)

    reserved_host_ports = set()
    intervm_roles = [None, None]
    if intervm.enabled:
        intervm_port = allocate_free_host_port(intervm.host_port, reserved=reserved_host_ports)
        intervm.host_port = intervm_port
        intervm_roles = [("listen", intervm_port), ("connect", intervm_port)]

    for vm in vms:
        old_ssh_port = vm.ssh_port
        new_ssh_port = None
        for forwarding in vm.port_forwarding:
            new_port = allocate_free_host_port(forwarding.host_port, reserved=reserved_host_ports)
            if forwarding.host_port == old_ssh_port or forwarding.guest_port == 22:
                new_ssh_port = new_port
            forwarding.host_port = new_port
        if new_ssh_port is not None:
            vm.ssh_port = new_ssh_port
        else:
            vm.ssh_port = allocate_free_host_port(vm.ssh_port, reserved=reserved_host_ports)

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
        intervm_address=_INTERVM_ADDRESSES[0] if intervm.enabled else None,
        qmp_socket_path=qmp_socket_paths[0],
        dump_dir=dump_dir,
    ) as process_a:
        if intervm.enabled:
            wait_for_host_port_bound(intervm.host_port)
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
            intervm_address=_INTERVM_ADDRESSES[1] if intervm.enabled else None,
            qmp_socket_path=qmp_socket_paths[1],
            dump_dir=dump_dir,
        ) as process_b:
            if intervm.enabled:
                # only now, both qemus exist so the link between them is up
                process_a.configure_intervm_nic()
                process_b.configure_intervm_nic()
            yield [process_a, process_b]


@pytest.fixture(scope="session")
def console_a(_targets):
    """VM-A serial console (the shell on ser1). Works without guest networking."""
    return _targets[0].console


@pytest.fixture(scope="session")
def console_b(_targets):
    """VM-B serial console."""
    return _targets[1].console


@pytest.fixture(scope="session")
def vm_a(_targets):
    """VM-A process object (console, qmp diagnostics)."""
    return _targets[0]


@pytest.fixture(scope="session")
def vm_b(_targets):
    """VM-B process object."""
    return _targets[1]


@pytest.fixture(scope="session")
def intervm_host_port(config, _targets):
    """Host port of the socket linking the two VMs, None if disabled."""
    intervm = config.dual_config.intervm_network
    return intervm.host_port if intervm.enabled else None


@pytest.fixture(scope="session")
def target_a(_targets):
    """The first VM (VM-A) in the inter-VM shared-memory tests."""
    return _targets[0].target


@pytest.fixture(scope="session")
def target_b(_targets):
    """The second VM (VM-B) in the inter-VM shared-memory tests."""
    return _targets[1].target
