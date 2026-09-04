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

The ``start()`` method is self-healing: it waits for stable SSH, runs ``pre_tests_phase``,
and restarts the QEMU process up to ``max_boot_attempts`` times if sshd never comes up.

All timeouts/retry counts below can be overridden via environment variables (e.g. via Bazel's
``--test_env``) without a code change, for CI hosts where nested virtualization makes the
guest's SSH authentication handshake much slower than on bare-metal KVM.
"""

import logging
import os
import time

from score.itf.plugins.qemu.checks import pre_tests_phase
from score.itf.plugins.qemu.qemu_process import QemuProcess
from score.itf.plugins.qemu.qemu_target import QemuTarget

from .ivshmem_qemu import IvshmemQemu

logger = logging.getLogger(__name__)


def _env_int(name: str, default: int) -> int:
    """Read an int from the environment, falling back to ``default`` if unset/invalid."""
    try:
        return int(os.environ.get(name, default))
    except (TypeError, ValueError):
        return default


def _wait_for_ssh(target, total_timeout: int = 60, interval: int = 1, stable_successes: int = 2):
    """Wait until the VM *stably* serves SSH.

    Early-boot sshd is briefly unstable, so require several consecutive successes to
    avoid the ``pre_tests_phase`` (5 retries) failing in that window.
    """
    deadline = time.monotonic() + total_timeout
    last_error = None
    consecutive = 0
    ssh_probe_timeout = _env_int("DUAL_QEMU_SSH_PROBE_TIMEOUT_S", 5)
    while time.monotonic() < deadline:
        try:
            with target.ssh(timeout=ssh_probe_timeout, n_retries=1, retry_interval=1) as ssh:
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
        max_boot_attempts=None,
        boot_timeout=None,
        peer=None,
    ):
        max_boot_attempts = max_boot_attempts or _env_int("DUAL_QEMU_MAX_BOOT_ATTEMPTS", 3)
        boot_timeout = boot_timeout or _env_int("DUAL_QEMU_BOOT_TIMEOUT_S", 60)
        super().__init__(
            path_to_qemu_image,
            available_ram,
            available_cores,
            port_forwarding=port_forwarding,
        )
        # Replace the base's default Qemu with our ivshmem-capable subclass.
        self._qemu = IvshmemQemu(
            path_to_qemu_image,
            available_ram,
            available_cores,
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
        # The intervm socket netdev pairs both VMs' host-side sockets once at QEMU
        # process start; restarting only one side leaves the other stale.
        self._peer = peer

    def set_peer(self, peer):
        """Set the intervm peer VM, restarted alongside this one when either goes bad."""
        self._peer = peer

    def start(self):
        """Boot the VM, retrying up to ``max_boot_attempts`` times if sshd never serves."""
        last_error = None
        for attempt in range(1, self._max_boot_attempts + 1):
            super().start()
            try:
                self._target = QemuTarget(self, self._vm_config)
                _wait_for_ssh(self._target, total_timeout=self._boot_timeout)
                pre_tests_phase(self._target)
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
                    if self._peer is not None:
                        logger.info("Restarting intervm peer VM to re-pair its socket netdev")
                        self._peer.restart()
                    logger.info("Waiting 5 s before next boot attempt to let resources settle")
                    time.sleep(5)
        raise RuntimeError(
            f"VM never booted into a usable state after {self._max_boot_attempts} attempts: {last_error}"
        )

    def ensure_responsive(self, timeout: int = None, stable_successes: int = 2):
        """Re-verify the VM is still reachable; restart (with its peer) in place if not."""
        timeout = timeout or _env_int("DUAL_QEMU_ENSURE_RESPONSIVE_TIMEOUT_S", 20)
        try:
            _wait_for_ssh(self._target, total_timeout=timeout, stable_successes=stable_successes)
        except Exception as ex:  # pylint: disable=broad-except
            logger.warning("VM went unresponsive (%s); restarting", ex)
            self.restart_with_peer()

    def restart_with_peer(self):
        """Restart this VM and, if an intervm peer is set, restart it too.

        Both restarts must happen together so the one-shot intervm socket netdev is
        re-paired on both sides; restarting only one side leaves the other stale.
        """
        peer = self._peer
        self._peer = None
        peer_saved = None
        if peer is not None:
            peer_saved = peer._peer  # pylint: disable=protected-access
            peer._peer = None  # pylint: disable=protected-access
        try:
            self.restart()
            if peer is not None:
                peer.restart()
        finally:
            self._peer = peer
            if peer is not None:
                peer._peer = peer_saved  # pylint: disable=protected-access

    @property
    def target(self):
        """The ``QemuTarget`` for this VM (available after ``start()``)."""
        return self._target
