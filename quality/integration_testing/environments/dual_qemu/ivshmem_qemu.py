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
"""A QEMU launcher that adds an ``ivshmem-plain`` device so VMs can share one host file.

Subclasses the upstream :class:`Qemu` and overrides :meth:`_extra_qemu_args` to inject
the ivshmem, inter-VM NIC, and per-VM MAC arguments.
"""
import logging

from score.itf.plugins.qemu.qemu import Qemu

logger = logging.getLogger(__name__)


class IvshmemQemu(Qemu):
    """A :class:`Qemu` subclass that maps a shared ``ivshmem-plain`` region."""

    def __init__(
        self,
        path_to_image,
        ram="1G",
        cores="2",
        port_forwarding=[],
        ivshmem_path=None,
        ivshmem_size="4M",
        intervm=None,
        vm_index=0,
    ):
        """
        :param str ivshmem_path: Host backing file shared between VMs (memory-backend-file).
        :param str ivshmem_size: Size of the shared region, e.g. "4M".
        :param tuple intervm: Optional ("listen"|"connect", host_port) for a point-to-point
            socket NIC between the two VMs. ``None`` disables it.
        :param int vm_index: Zero-based VM index, used to derive unique NIC MACs.
        """
        self._ivshmem_path = ivshmem_path
        self._ivshmem_size = ivshmem_size
        self._intervm = intervm
        self._vm_index = vm_index
        self._dual_port_forwarding = port_forwarding
        # Pass port_forwarding=[] to the base so it doesn't add default-MAC devices.
        # We handle port forwarding ourselves in _extra_qemu_args with per-VM MACs.
        super().__init__(path_to_image, ram, cores, cpu="host", port_forwarding=[])
        # Re-resolve: "host" is invalid under TCG, fall back to "max".
        if self._accelerator_support == "tcg":
            self._Qemu__cpu = "max"
            logger.warning("Running under TCG: using -cpu max instead of host.")

    def _extra_qemu_args(self):
        """Inject ivshmem, per-VM-MAC port forwarding, and inter-VM NIC arguments."""
        return self._ivshmem_args() + self._port_forwarding_with_mac_args() + self._intervm_args()

    def _ivshmem_args(self):
        if not self._ivshmem_path:
            return []
        # ivshmem-plain: a passive shared-memory PCI device (no MSI-X / doorbell).
        # Both VMs point "mem-path" at the same host file with share=on, so the device
        # BAR2 of each guest is backed by the same physical pages.
        return [
            "-object",
            (
                "memory-backend-file,id=ivshmem_mem,"
                f"mem-path={self._ivshmem_path},size={self._ivshmem_size},share=on"
            ),
            "-device",
            "ivshmem-plain,memdev=ivshmem_mem",
        ]

    def _mac_for(self, nic_id):
        """Locally-administered MAC unique per (vm_index, nic_id)."""
        return f"52:54:00:00:{self._vm_index:02x}:{nic_id:02x}"

    def _port_forwarding_with_mac_args(self):
        """Port forwarding with per-VM unique MACs.

        The base class's ``__port_forwarding_args`` uses QEMU's default MAC, which causes
        the second QNX VM's sshd to hang when two VMs run on the same host. We pass
        ``port_forwarding=[]`` to the base and handle it here with unique MACs instead.
        """
        result = []
        for id, forwarding in enumerate(self._dual_port_forwarding, start=1):
            result.extend(
                [
                    "-netdev",
                    f"user,id=net{id},hostfwd=tcp::{forwarding.host_port}-:{forwarding.guest_port}",
                    "-device",
                    f"virtio-net-pci,netdev=net{id},mac={self._mac_for(id)}",
                ]
            )
        return result

    def _intervm_args(self):
        if not self._intervm:
            return []
        mode, host_port = self._intervm
        if mode == "listen":
            netdev = f"socket,id=intervm,listen=:{host_port}"
        else:
            netdev = f"socket,id=intervm,connect=127.0.0.1:{host_port}"
        return [
            "-netdev",
            netdev,
            "-device",
            f"virtio-net-pci,netdev=intervm,mac={self._mac_for(0x80)}",
        ]
