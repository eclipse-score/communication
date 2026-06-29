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

The ``Qemu.__build_qemu_command`` is name-mangled with no extension point for
extra ``-object`` / ``-device`` flags, so the command is rebuilt here.
"""
import logging
import os
import subprocess
import sys

logger = logging.getLogger(__name__)


class IvshmemQemu:
    """Start a QEMU instance that maps a shared ``ivshmem-plain`` region."""

    def __init__(
        self,
        path_to_image,
        ram="1G",
        cores="2",
        cpu=None,
        port_forwarding=[],
        ivshmem_path=None,
        ivshmem_size="4M",
        intervm=None,
        vm_index=0,
    ):
        """
        :param str path_to_image: Path to the QEMU IFS image.
        :param str ram: RAM to allocate.
        :param str cores: Number of CPU cores.
        :param str cpu: CPU model to emulate. When ``None`` (default) it is chosen to match
            the accelerator: ``host`` under KVM, ``max`` under TCG. Passing ``host`` with
            TCG makes QEMU warn about host CPU features it cannot emulate.
        :param list port_forwarding: Host<->guest TCP forwards (e.g. SSH).
        :param str ivshmem_path: Host backing file shared between VMs (memory-backend-file).
        :param str ivshmem_size: Size of the shared region, e.g. "4M".
        :param tuple intervm: Optional ("listen"|"connect", host_port) describing a
            point-to-point socket NIC between the two VMs. ``None`` disables it.
        :param int vm_index: Zero-based VM index. Used to derive a unique NIC MAC so the
            two VMs never share QEMU's default MAC (a duplicate MAC makes the second QNX
            VM's sshd hang under SLIRP user networking).
        """
        self.__qemu_path = "/usr/bin/qemu-system-x86_64"
        self.__path_to_image = path_to_image
        self.__ram = ram
        self.__cores = cores
        self.__cpu = cpu
        self.__port_forwarding = port_forwarding
        self.__ivshmem_path = ivshmem_path
        self.__ivshmem_size = ivshmem_size
        self.__intervm = intervm
        self.__vm_index = vm_index

        self.__check_qemu_is_installed()
        self.__find_available_kvm_support()
        self.__check_kvm_readable_when_necessary()
        self.__resolve_cpu_model()

        self._subprocess = None

    def __enter__(self):
        return self.start()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()

    def start(self, subprocess_params=None):
        command = self.__build_qemu_command()
        logger.debug(command)
        subprocess_args = {"args": command}
        if subprocess_params:
            subprocess_args.update(subprocess_params)
        self._subprocess = subprocess.Popen(**subprocess_args)
        return self._subprocess

    def stop(self):
        if self._subprocess.poll() is None:
            self._subprocess.terminate()
            self._subprocess.wait(2)
        if self._subprocess.poll() is None:
            self._subprocess.kill()
            self._subprocess.wait(2)
        ret = self._subprocess.returncode
        if ret != 0:
            raise Exception(f"QEMU process returned: {ret}")

    def __check_qemu_is_installed(self):
        if not os.path.isfile(self.__qemu_path):
            logger.fatal(f"Qemu is not installed under {self.__qemu_path}")
            sys.exit(-1)

    def __find_available_kvm_support(self):
        self._accelerator_support = "kvm"
        with open("/proc/cpuinfo") as cpuinfo:
            cpu_options = str(cpuinfo.read())
            if "vmx" not in cpu_options and "svm" not in cpu_options:
                logger.error("No virtual capability on machine. We're using standard TCG accel on QEMU")
                self._accelerator_support = "tcg"

            if not os.path.exists("/dev/kvm"):
                logger.error("No KVM available. We're using standard TCG accel on QEMU")
                self._accelerator_support = "tcg"

    def __check_kvm_readable_when_necessary(self):
        if self._accelerator_support == "kvm":
            if not os.access("/dev/kvm", os.R_OK):
                logger.fatal(
                    "You dont have access rights to /dev/kvm. Consider adding yourself to kvm group. Aborting."
                )
                sys.exit(-1)

    def __resolve_cpu_model(self):
        # Match the CPU model to the accelerator: "host" passes through the real CPU under
        # KVM, but is invalid under TCG (which cannot emulate arbitrary host features), so
        # fall back to "max". An explicit cpu= from the caller is always honoured.
        if self.__cpu is None:
            self.__cpu = "host" if self._accelerator_support == "kvm" else "max"
        if self._accelerator_support == "tcg":
            logger.warning(
                "Running QEMU under TCG (no KVM): the QNX guest will boot much slower; "
                "using -cpu %s.",
                self.__cpu,
            )

    def __build_qemu_command(self):
        serial = ["-serial", "mon:stdio"]
        dbg = os.environ.get("DUAL_QEMU_SERIAL_DIR")
        if dbg:
            serial = ["-serial", f"file:{dbg}/vm{self.__vm_index}_serial.log"]
        accel = ["--enable-kvm"] if self._accelerator_support == "kvm" else ["-accel", "tcg"]
        return (
            [
                f"{self.__qemu_path}",
            ]
            + accel
            + [
                "-smp",
                f"{self.__cores},maxcpus={self.__cores},cores={self.__cores}",
                "-cpu",
                f"{self.__cpu}",
                "-m",
                f"{self.__ram}",
                "-kernel",
                f"{self.__path_to_image}",
                "-nographic",
            ]
            + serial
            + [
                "-object",
                "rng-random,filename=/dev/urandom,id=rng0",
                "-device",
                "virtio-rng-pci,rng=rng0",
            ]
            + self.__ivshmem_args()
            + self.__port_forwarding_args()
            + self.__intervm_args()
        )

    def __ivshmem_args(self):
        if not self.__ivshmem_path:
            return []
        # ivshmem-plain: a passive shared-memory PCI device (no MSI-X / doorbell).
        # Both VMs point "mem-path" at the same host file with share=on, so the device
        # BAR2 of each guest is backed by the same physical pages.
        return [
            "-object",
            (
                "memory-backend-file,id=ivshmem_mem,"
                f"mem-path={self.__ivshmem_path},size={self.__ivshmem_size},share=on"
            ),
            "-device",
            "ivshmem-plain,memdev=ivshmem_mem",
        ]

    def __mac_for(self, nic_id):
        # Locally-administered MAC unique per (vm_index, nic_id). Two VMs sharing QEMU's
        # default MAC (52:54:00:12:34:56) makes the second QNX VM's sshd hang.
        return f"52:54:00:00:{self.__vm_index:02x}:{nic_id:02x}"

    def __port_forwarding_args(self):
        result = []
        for id, forwarding in enumerate(self.__port_forwarding, start=1):
            result.extend(
                [
                    "-netdev",
                    f"user,id=net{id},hostfwd=tcp::{forwarding.host_port}-:{forwarding.guest_port}",
                    "-device",
                    f"virtio-net-pci,netdev=net{id},mac={self.__mac_for(id)}",
                ]
            )
        return result

    def __intervm_args(self):
        if not self.__intervm:
            return []
        mode, host_port = self.__intervm
        if mode == "listen":
            netdev = f"socket,id=intervm,listen=:{host_port}"
        else:
            netdev = f"socket,id=intervm,connect=127.0.0.1:{host_port}"
        return [
            "-netdev",
            netdev,
            "-device",
            f"virtio-net-pci,netdev=intervm,mac={self.__mac_for(0x80)}",
        ]
