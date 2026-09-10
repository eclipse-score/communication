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
"""Process wrapper that extends :class:`QemuProcess` with ivshmem and boot-retry logic.

Subclasses ``QemuProcess`` and replaces its internal ``_qemu`` with an
:class:`IvshmemQemu` instance so the VM is launched with an ``ivshmem-plain`` device.

The ``start()`` method is self-healing: it waits for stable SSH and restarts the QEMU
process up to ``max_boot_attempts`` times if sshd never comes up.
"""

import logging
import time

from score.itf.plugins.qemu.qemu_process import QemuProcess
from score.itf.plugins.qemu.qemu_target import QemuTarget

from .ivshmem_qemu import IvshmemQemu

logger = logging.getLogger(__name__)


def _wait_for_ssh(target, total_timeout: int = 180, interval: int = 3, stable_successes: int = 3):
    """Wait until the VM *stably* serves SSH.

    Early-boot sshd is briefly unstable, so require several consecutive successes to
    avoid the ``pre_tests_phase`` (5 retries) failing in that window. Reuse one SSH
    connection for the consecutive checks because this guest can fail to accept a new
    connection while an existing one is open.
    """
    deadline = time.monotonic() + total_timeout
    last_error = None
    while time.monotonic() < deadline:
        consecutive = 0
        try:
            with target.ssh(timeout=10, n_retries=1, retry_interval=1) as ssh:
                while consecutive < stable_successes:
                    return_code = ssh.execute_command("echo ready")
                    if return_code != 0:
                        last_error = RuntimeError(f"SSH readiness command failed with exit code {return_code}")
                        break
                    consecutive += 1
                    if consecutive >= stable_successes:
                        return
                    time.sleep(interval)
        except Exception as ex:  # pylint: disable=broad-except
            last_error = ex
        time.sleep(interval)
    raise TimeoutError(f"VM never became stably reachable via SSH within {total_timeout}s: {last_error}")


class DualQemuProcess(QemuProcess):
    """A :class:`QemuProcess` subclass with ivshmem support and self-healing boot.

    Use as a context manager::

        with DualQemuProcess(...) as process:
            target = process.target
            ...
    """

    def __init__(
        self,
        path_to_qemu_image,
        available_ram,
        available_cores,
        vm_config,
        port_forwarding=[],
        ivshmem_path=None,
        ivshmem_size="4M",
        intervm=None,
        vm_index=0,
        max_boot_attempts=3,
        boot_timeout=180,
    ):
        super().__init__(
            path_to_qemu_image,
            available_ram,
            available_cores,
            network_adapters=[],
            port_forwarding=port_forwarding,
            machine=vm_config.qemu_machine,
            rootfs=None,
            kernel_cmdline=vm_config.qemu_kernel_cmdline,
        )
        # Replace the base's default Qemu with our ivshmem-capable subclass.
        self._qemu = IvshmemQemu(
            path_to_qemu_image,
            available_ram,
            available_cores,
            network_adapters=[],
            port_forwarding=port_forwarding,
            ivshmem_path=ivshmem_path,
            ivshmem_size=ivshmem_size,
            intervm=intervm,
            vm_index=vm_index,
        )
        self._vm_config = vm_config
        self._max_boot_attempts = max_boot_attempts
        self._boot_timeout = boot_timeout
        self._target = None

    def start(self):
        """Boot the VM, retrying up to ``max_boot_attempts`` times if sshd never serves."""
        last_error = None
        for attempt in range(1, self._max_boot_attempts + 1):
            super().start()
            try:
                self._target = QemuTarget(self, self._vm_config)
                _wait_for_ssh(self._target, total_timeout=self._boot_timeout)
                return self
            except Exception as ex:  # pylint: disable=broad-except
                last_error = ex
                logger.warning(
                    "VM boot attempt %d/%d did not reach a usable state (%s); restarting",
                    attempt,
                    self._max_boot_attempts,
                    ex,
                )
                try:
                    self.stop()
                except Exception:  # pylint: disable=broad-except
                    logger.exception("Failed to stop the wedged QEMU before retrying")
                if attempt < self._max_boot_attempts:
                    logger.info("Waiting 5 s before next boot attempt to let resources settle")
                    time.sleep(5)
        raise RuntimeError(
            f"VM never booted into a usable state after {self._max_boot_attempts} attempts: {last_error}"
        )

    def is_responsive(self, timeout: int = 60, stable_successes: int = 2) -> bool:
        """Read-only SSH reachability probe (no restart); safe to run concurrently for both VMs."""
        try:
            _wait_for_ssh(self._target, total_timeout=timeout, stable_successes=stable_successes)
            return True
        except TimeoutError:
            return False

    def self_heal(self):
        """Restart the VM, reusing start()'s own boot-retry + readiness-check loop.

        A VM that already passed ``start()`` can still stop serving sshd while it sits idle during
        the peer's boot; one restart recovers it without needing a full outer Bazel retry, which
        would reboot both VMs from scratch.
        """
        logger.warning("VM went unresponsive; restarting to self-heal")
        self.stop()
        self.start()

    def ensure_responsive(self, timeout: int = 60, stable_successes: int = 2):
        """Re-verify the VM is still reachable, restarting it (self-heal) if it went idle-dead."""
        if not self.is_responsive(timeout, stable_successes):
            self.self_heal()

    @property
    def target(self):
        """The ``QemuTarget`` for this VM (available after ``start()``)."""
        return self._target
