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
        network_adapters=None,
        port_forwarding=[],
        machine="pc-x86_64",
        rootfs=None,
        kernel_cmdline=None,
        ivshmem_path=None,
        ivshmem_size="4M",
        intervm=None,
        vm_index=0,
        qmp_socket_path=None,
        dump_dir=None,
    ):
        """
        :param str ivshmem_path: Host backing file shared between VMs (memory-backend-file).
        :param str ivshmem_size: Size of the shared region, e.g. "4M".
        :param tuple intervm: Optional ("listen"|"connect", host_port) for a point-to-point
            socket NIC between the two VMs. ``None`` disables it.
        :param int vm_index: Zero-based VM index, used to derive unique NIC MACs.
        :param str qmp_socket_path: Optional QMP unix socket, for diagnostics.
        :param str dump_dir: Optional directory for a pcap per netdev.
        """
        self._ivshmem_path = ivshmem_path
        self._ivshmem_size = ivshmem_size
        self._intervm = intervm
        self._vm_index = vm_index
        self._qmp_socket_path = qmp_socket_path
        self._dump_dir = dump_dir
        network_adapters = network_adapters if network_adapters is not None else []
        self._network_adapters = network_adapters
        self._dual_port_forwarding = port_forwarding
        # Pass port_forwarding=[] to the base so it doesn't add default-MAC devices.
        # We handle port forwarding ourselves in _extra_qemu_args with per-VM MACs.
        super().__init__(
            path_to_image,
            ram,
            cores,
            machine=machine,
            network_adapters=[],
            port_forwarding=[],
            rootfs=rootfs,
            kernel_cmdline=kernel_cmdline,
        )

    def _extra_qemu_args(self):
        """Inject ivshmem, per-VM-MAC port forwarding, inter-VM NIC, qmp and pcap arguments."""
        return (
            self._ivshmem_args()
            + self._port_forwarding_with_mac_args()
            + self._intervm_args()
            + self._qmp_args()
            + self._dump_args()
        )

    def _qmp_args(self):
        if not self._qmp_socket_path:
            return []
        return ["-qmp", f"unix:{self._qmp_socket_path},server=on,wait=off"]

    def _dump_args(self):
        """pcap per netdev, both directions, taken at the netdev side of the NIC."""
        if not self._dump_dir:
            return []
        netdev_ids = [f"net{id}" for id, _ in enumerate(self._dual_port_forwarding, start=1)]
        if self._intervm:
            netdev_ids.append("intervm")
        result = []
        for netdev_id in netdev_ids:
            result.extend(
                [
                    "-object",
                    f"filter-dump,id=dump_{netdev_id},netdev={netdev_id},"
                    f"file={self._dump_dir}/vm{self._vm_index}_{netdev_id}.pcap",
                ]
            )
        return result

    def _ivshmem_args(self):
        if not self._ivshmem_path:
            return []
        # ivshmem-plain: a passive shared-memory PCI device (no MSI-X / doorbell).
        # Both VMs point "mem-path" at the same host file with share=on, so the device
        # BAR2 of each guest is backed by the same physical pages.
        return [
            "-object",
            (f"memory-backend-file,id=ivshmem_mem,mem-path={self._ivshmem_path},size={self._ivshmem_size},share=on"),
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
            netdev = f"stream,id=intervm,server=on,addr.type=inet,addr.host=127.0.0.1,addr.port={host_port}"
        else:
            netdev = f"stream,id=intervm,addr.type=inet,addr.host=127.0.0.1,addr.port={host_port},reconnect=1"
        return [
            "-netdev",
            netdev,
            "-device",
            f"virtio-net-pci,netdev=intervm,mac={self._mac_for(0x80)}",
        ]
