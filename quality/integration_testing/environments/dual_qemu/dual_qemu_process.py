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
import socket
import time

from score.itf.plugins.qemu.qemu_process import QemuProcess
from score.itf.plugins.qemu.qemu_target import QemuTarget

from .ivshmem_qemu import IvshmemQemu

logger = logging.getLogger(__name__)


def _host_port_is_free(port: int, host: str = "127.0.0.1") -> bool:
    # SO_REUSEADDR keeps TIME_WAIT leftovers from looking like a live listener.
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            probe.bind((host, port))
        except OSError:
            return False
    return True


def allocate_free_host_port(
    preferred_port: int | None = None,
    host: str = "127.0.0.1",
    reserved: set[int] | None = None,
) -> int:
    """Allocate a free ephemeral port from the OS to avoid TIME_WAIT collisions across runs."""
    if reserved is None:
        reserved = set()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        probe.bind((host, 0))
        port = probe.getsockname()[1]
        while port in reserved or not _host_port_is_free(port, host):
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe2:
                probe2.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                probe2.bind((host, 0))
                port = probe2.getsockname()[1]
        reserved.add(port)
        if preferred_port is not None:
            logger.info(
                "Host port %d on %s mapped to dynamic free port %d",
                preferred_port,
                host,
                port,
            )
        return port


def require_free_host_ports(ports, host: str = "127.0.0.1"):
    """Reject ports a killed run still holds; QEMU would otherwise fail to bind and strand the guests."""
    taken = sorted({port for port in ports if not _host_port_is_free(port, host)})
    if taken:
        raise RuntimeError(
            f"Host ports already in use on {host}: {taken}. "
            "A previous QEMU run was most likely interrupted; stop the stale process before retrying."
        )


def wait_for_host_port_bound(port: int, host: str = "127.0.0.1", timeout_s: int = 30):
    """Block until QEMU owns the listener, so the connecting VM never retries a socket that was never created."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if not _host_port_is_free(port, host):
            return
        time.sleep(0.2)
    raise TimeoutError(f"QEMU never bound the inter-VM socket {host}:{port} within {timeout_s}s")


def _wait_for_ssh(target, total_timeout: int = 180, interval: int = 1, stable_successes: int = 3):
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


def execute_async_with_retries(
    target,
    binary_path,
    attempts: int = 3,
    ssh_recovery_timeout_s: int = 30,
    **kwargs,
):
    """Retry application launch when the guest SSH session is transiently unavailable."""
    last_error = None
    for attempt in range(1, attempts + 1):
        if attempt > 1:
            try:
                _wait_for_ssh(
                    target,
                    total_timeout=ssh_recovery_timeout_s,
                    stable_successes=2,
                )
            except Exception as probe_error:  # pylint: disable=broad-except
                logger.warning(
                    "VM still not serving SSH before retry %d (%s)",
                    attempt,
                    probe_error,
                )
        try:
            return target.execute_async(binary_path, **kwargs)
        except Exception as ex:  # pylint: disable=broad-except
            last_error = ex
            logger.warning(
                "Launching %s failed on attempt %d/%d (%s)",
                binary_path,
                attempt,
                attempts,
                ex,
            )
    raise last_error


def stop_quietly(process, label: str = ""):
    """Keep remote-process cleanup from masking the test result."""
    try:
        process.stop()
    except Exception as ex:  # pylint: disable=broad-except
        logger.warning(
            "Could not stop remote process %s cleanly (%s)",
            label,
            ex,
        )


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

    def ensure_responsive(self, timeout: int = 30, stable_successes: int = 2):
        """Re-verify the VM is still reachable; restart in place if not."""
        try:
            _wait_for_ssh(self._target, total_timeout=timeout, stable_successes=stable_successes)
        except Exception as ex:  # pylint: disable=broad-except
            logger.warning("VM went unresponsive (%s); restarting", ex)
            self.restart()

    @property
    def target(self):
        """The ``QemuTarget`` for this VM (available after ``start()``)."""
        return self._target
