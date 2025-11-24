# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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
import errno
import os
import sys
import json
import socket
import logging
import subprocess
import threading
import time

from quality.integration_testing.environments.qnx8_qemu.console import PipeConsole


class QemuException(Exception):
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"QEMU: {repr(self.value)}"


class QEMURunner:
    QMP_ADDRESS = ("127.0.0.1", 4242)
    QEMU_BINARY = "qemu-system-x86_64"

    def _find_available_kvm_support(self):
        self._accelerator_support = "--enable-kvm"
        with open("/proc/cpuinfo") as cpuinfo:
            cpu_options = str(cpuinfo.read())
            if "vmx" not in cpu_options and "svm" not in cpu_options:
                logging.error("No virtual capability on machine. We're using standard accel on QEMU")
                self._accelerator_support = ""

            if not os.path.exists("/dev/kvm"):
                logging.error("No KVM available. We're using standard  accel on QEMU")
                self._accelerator_support = ""

    def _check_kvm_readable_when_necessary(self):
        if self._accelerator_support == "kvm":
            if not os.access('/dev/kvm', os.R_OK):
                logging.fatal(
                    "You dont have access rights to /dev/kvm. Consider adding yourself to kvm group. Aborting.")
                sys.exit(-1)

    def __run_qemu(self, cmdline):
        stdin = subprocess.PIPE
        stdout = subprocess.PIPE
        self._qemu = subprocess.Popen(cmdline, shell=False, bufsize=16384,
                                      stdin=stdin, stdout=stdout, close_fds=True)
        self._qemu.wait()
        logging.info("QEMU has been stopped.\n")

    def __init__(self, firmware):
        self._find_available_kvm_support()
        self._check_kvm_readable_when_necessary()

        qemu_options = ["-smp", "2",
                        "-m", "2G",
                        "-nographic",
                        "-serial", "stdio",
                        "-object", "rng-random,filename=/dev/urandom,id=rng0",
                        "-device", "virtio-rng-pci,rng=rng0",
                        f"{self._accelerator_support}",
                        "-cpu", "Cascadelake-Server-v5"
                        ]

        self._listen_for_qmp_socket()
        self._start_qemu_in_background(
            [self.QEMU_BINARY] + qemu_options + ["-kernel", firmware, "-qmp", "tcp:%s:%d" % self.QMP_ADDRESS])

        try:
            logging.info("Waiting for QEMU connection\n")
            self._socket, _ = self._socket.accept()
            self._socket_file_descriptor = self._socket.makefile()
        except socket.error:
            raise QemuException('Cannot connect to QMP server')

        self._ensure_qmp_supported()

        self.console = PipeConsole("Qemu", self._qemu)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def _listen_for_qmp_socket(self):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.bind(self.QMP_ADDRESS)
        self._socket.listen(1)

    def _start_qemu_in_background(self, args):
        self._worker_thread = threading.Thread(target=QEMURunner.__run_qemu, args=(self, args))
        self._worker_thread.start()

    def _ensure_qmp_supported(self):
        version = self._receive_qmp_data()
        if version is None or 'QMP' not in version:
            raise QemuException(f'Not QMP support: {version}')

    def _initialize_qmp(self):
        resp = self.send_qmp('qmp_capabilities')
        if not "return" in resp:
            raise QemuException('QMP not working properly')
        logging.info("QMP connected\n")

    def _receive_qmp_data(self):
        while True:
            data = self._socket_file_descriptor.readline()
            if not data:
                return json.loads("{}")
            return json.loads(data)

    def send_qmp(self, name, args=None):
        command = {'execute': name}
        if args:
            command['arguments'] = args
        try:
            self._socket.sendall(json.dumps(command).encode('utf-8'))
        except socket.error as err:
            if err[0] == errno.EPIPE:
                return None
            raise QemuException("Error on QMP socket:" + err)
        return self._receive_qmp_data()

    def serial_readline(self):
        return self._qemu.stdout.readline()

    def serial_write(self, string):
        self._qemu.stdin.write(string.encode('utf-8'))
        self._qemu.stdin.flush()

    def get_event(self, blocking=True):
        if not blocking:
            self._socket.setblocking(0)
        try:
            val = self._receive_qmp_data()
        except socket.error as err:
            if err[0] == errno.EAGAIN:
                return None
            else:
                raise QemuException("Error on QEMU socket:" + err)
        if not blocking:
            self._socket.setblocking(1)
        return val

    def _gracefully_terminate(self):
        if self._qemu.poll() is None:
            self.send_qmp("quit")
            time.sleep(0.1)

    def _force_terminate_if_necessary(self):
        if self._qemu.poll() is None:
            self._qemu.terminate()

    def close(self):
        self._gracefully_terminate()
        self._force_terminate_if_necessary()
        self._worker_thread.join()
        self._socket.close()
        self._socket_file_descriptor.close()
