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
"""Process wrapper around :class:`IvshmemQemu`, mirroring the ``QemuProcess``.

It only differs from ``QemuProcess`` in that it drives :class:`IvshmemQemu` (so the VM is
launched with an ``ivshmem-plain`` device) instead of the plain ``Qemu``.
"""
import logging
import subprocess

from score.itf.core.process.console import PipeConsole

from .ivshmem_qemu import IvshmemQemu

logger = logging.getLogger(__name__)


class DualQemuProcess:
    def __init__(
        self,
        path_to_qemu_image,
        available_ram,
        available_cores,
        port_forwarding=[],
        ivshmem_path=None,
        ivshmem_size="4M",
        intervm=None,
        vm_index=0,
    ):
        self._path_to_qemu_image = path_to_qemu_image
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
        self._console = None

    def __enter__(self):
        return self.start()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()

    def start(self):
        logger.info(f"Starting Qemu... (image: {self._path_to_qemu_image})")
        subprocess_params = {
            "stdin": subprocess.PIPE,
            "stdout": subprocess.PIPE,
            "stderr": subprocess.STDOUT,
        }
        qemu_subprocess = self._qemu.start(subprocess_params)
        self._console = PipeConsole("QEMU", qemu_subprocess)
        return self

    def stop(self):
        logger.info("Stopping Qemu...")
        self._qemu.stop()

    def restart(self):
        self.stop()
        self.start()

    @property
    def console(self):
        return self._console
