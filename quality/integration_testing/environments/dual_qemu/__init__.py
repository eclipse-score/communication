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
import time

import pytest

from score.itf.core.utils.bunch import Bunch
from score.itf.plugins.qemu.checks import pre_tests_phase
from score.itf.plugins.qemu.qemu_target import QemuTarget

from .config import load_configuration, parse_size
from .dual_qemu_process import DualQemuProcess

logger = logging.getLogger(__name__)


def _wait_for_ssh(target, total_timeout: int = 180, interval: int = 3, stable_successes: int = 3):
    """Wait until the VM *stably* serves SSH.

    Early-boot sshd is briefly unstable, so require several consecutive successes to
    avoid the ``pre_tests_phase`` (5 retries) failing in that window.
    """
    deadline = time.monotonic() + total_timeout
    last_error = None
    consecutive = 0
    while time.monotonic() < deadline:
        try:
            with target.ssh(timeout=10, n_retries=1, retry_interval=1) as ssh:
                if ssh.execute_command("echo ready") == 0:
                    consecutive += 1
                    if consecutive >= stable_successes:
                        return
                    time.sleep(interval)
                    continue
            consecutive = 0
        except Exception as ex:  # pylint: disable=broad-except
            last_error = ex
            consecutive = 0
        time.sleep(interval)
    raise TimeoutError(f"VM never became stably reachable via SSH within {total_timeout}s: {last_error}")


def _boot_vm(make_process, vm, *, boot_timeout: int = 100, max_boot_attempts: int = 3):
    """Boot a VM until it stably serves SSH and passes ``pre_tests_phase``.

    A guest occasionally boots but its sshd never serves; that boot is unrecoverable,
    so restart the guest up to ``max_boot_attempts`` times. Returns the live
    ``(process, target)``; the caller stops the process.
    """
    last_error = None
    for attempt in range(1, max_boot_attempts + 1):
        process = make_process()
        try:
            target = QemuTarget(process, vm)
            _wait_for_ssh(target, total_timeout=boot_timeout)
            pre_tests_phase(target)
            return process, target
        except Exception as ex:  # pylint: disable=broad-except
            last_error = ex
            logger.warning(
                "VM boot attempt %d/%d did not reach a usable state (%s); restarting guest",
                attempt,
                max_boot_attempts,
                ex,
            )
            try:
                process.stop()
            except Exception:  # pylint: disable=broad-except
                logger.exception("Failed to stop the wedged QEMU before retrying")
    raise RuntimeError(
        f"VM never booted into a usable state after {max_boot_attempts} attempts: {last_error}"
    )


def pytest_addoption(parser):
    parser.addoption(
        "--dual-qemu-config",
        action="store",
        required=True,
        help="Path to a JSON file describing the two VMs and the ivshmem region.",
    )
    parser.addoption(
        "--qemu-image",
        action="store",
        required=True,
        help="Path to the (shared) QEMU IFS image booted by both VMs.",
    )


@pytest.fixture(scope="session")
def config(request):
    return Bunch(
        dual_config=load_configuration(request.config.getoption("dual_qemu_config")),
        qemu_image=request.config.getoption("qemu_image"),
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
def targets(config, ivshmem_backend):
    """Boot both VMs in order, run the pre-test checks, and tear them down in reverse."""
    logger.info(f"Starting dual-VM tests on host: {socket.gethostname()}")
    dual_config = config.dual_config

    # When the inter-VM network is enabled, VM-A hosts the socket and VM-B connects to it,
    # so VM-A must be started first (it already is, by index order).
    intervm = dual_config.intervm_network
    intervm_roles = [None, None]
    if intervm.enabled:
        intervm_roles = [("listen", intervm.host_port), ("connect", intervm.host_port)]

    processes = []
    booted_targets = []
    try:
        # Boot the VMs one at a time: start a VM, wait until it is fully reachable, and
        # only then start the next one. Booting two QNX guests *concurrently* under KVM
        # occasionally leaves the second guest's device bring-up (SMP AP / virtio-net)
        # wedged; staggering the boots makes each guest initialize in isolation, exactly
        # like the single-VM tests, which is reliable. _boot_vm additionally restarts an
        # individual guest in place on the rare boot where its sshd never serves.
        makers = []
        for index, vm in enumerate(dual_config.vms):
            def make_process(index=index, vm=vm):
                return DualQemuProcess(
                    config.qemu_image,
                    vm.qemu_ram_size,
                    vm.qemu_num_cores,
                    port_forwarding=vm.port_forwarding,
                    ivshmem_path=ivshmem_backend,
                    ivshmem_size=dual_config.ivshmem.size,
                    intervm=intervm_roles[index],
                    vm_index=index,
                ).start()

            makers.append((make_process, vm))
            process, target = _boot_vm(make_process, vm)
            processes.append(process)
            booted_targets.append(target)

        # Final health gate: a VM that booted earlier may have gone quiet while the
        # later VM(s) booted (QNX sshd occasionally stops serving shortly after an idle
        # boot). Re-verify every VM right before handing control to the test and restart
        # any that is no longer stably reachable, so the test starts with all VMs live.
        for i, (make_process, vm) in enumerate(makers):
            try:
                _wait_for_ssh(booted_targets[i], total_timeout=30, stable_successes=2)
            except Exception as ex:  # pylint: disable=broad-except
                logger.warning("VM %d went unresponsive after boot (%s); restarting it", i, ex)
                try:
                    processes[i].stop()
                except Exception:  # pylint: disable=broad-except
                    logger.exception("Failed to stop the unresponsive QEMU before restart")
                process, target = _boot_vm(make_process, vm)
                processes[i] = process
                booted_targets[i] = target

        yield booted_targets
    finally:
        for process in reversed(processes):
            try:
                process.stop()
            except Exception:  # pylint: disable=broad-except
                logger.exception("Failed to stop a QEMU process during teardown")


@pytest.fixture(scope="session")
def target_a(targets):
    """The first VM (VM-A) in the inter-VM shared-memory tests."""
    return targets[0]


@pytest.fixture(scope="session")
def target_b(targets):
    """The second VM (VM-B) in the inter-VM shared-memory tests."""
    return targets[1]
