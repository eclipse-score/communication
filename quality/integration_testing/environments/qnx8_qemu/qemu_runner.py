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
import shutil
import sys
import json
import socket
import logging
import tempfile
import subprocess
import threading
import time

from quality.integration_testing.environments.qnx8_qemu.console import PipeConsole


class QemuException(Exception):
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"QEMU: {repr(self.value)}"


class SerialChannel:
    """Represents a dedicated QEMU serial port channel.

    Each channel consists of a host-side Unix socket and a corresponding
    guest-side device path (e.g. /dev/ser2).  QEMU acts as the server
    (``-serial unix:<path>,server=on,wait=off``), and the host connects
    as a client to read the guest process output.
    """

    def __init__(self, socket_path: str, guest_device: str) -> None:
        self.socket_path = socket_path
        self.guest_device = guest_device
        self._connection: socket.socket | None = None

    def connect(self, timeout: float = 30) -> None:
        """Connect to the QEMU-created Unix socket (retry until available)."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                sock.connect(self.socket_path)
                sock.setblocking(True)
                self._connection = sock
                return
            except (ConnectionRefusedError, FileNotFoundError, OSError):
                time.sleep(0.2)
        raise QemuException(
            f"Timed out connecting to QEMU serial socket: {self.socket_path}"
        )

    def makefile(self):
        """Return a file-like object for line-oriented reading."""
        return self._connection.makefile("rb")

    def close(self) -> None:
        if self._connection:
            self._connection.close()


class SerialChannelPool:
    """Manages a fixed pool of extra QEMU serial channels.

    Channels are pre-allocated at QEMU launch time.  Callers acquire a
    channel before starting a guest process and release it when done.
    """

    def __init__(self, channels: list[SerialChannel]) -> None:
        self._available = list(channels)
        self._lock = threading.Lock()

    def acquire(self) -> SerialChannel | None:
        """Return an available channel, or None if all channels are in use."""
        with self._lock:
            if not self._available:
                return None
            return self._available.pop()

    def release(self, channel: SerialChannel) -> None:
        with self._lock:
            self._available.append(channel)

    def close_all(self) -> None:
        with self._lock:
            for ch in self._available:
                ch.close()
            self._available.clear()


class QEMURunner:
    QMP_ADDRESS = ("127.0.0.1", 4242)
    QEMU_BINARY = "qemu-system-x86_64"
    # QEMU x86_64 default machine type emulates up to four ISA 8250 UARTs
    # at the standard COM port I/O addresses (COM1-COM4).
    # COM1 (-serial stdio) is the main console; COM2-COM4 are available
    # for dedicated process output channels (/dev/ser2 – /dev/ser4 in QNX).
    NUM_EXTRA_SERIAL_PORTS = 3

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

    def _create_serial_channels(self) -> tuple[list[SerialChannel], list[str]]:
        """Pre-create Unix sockets and build QEMU args for extra serial ports.

        Each extra port uses:
          -serial unix:<path>,server=on,wait=off

        QEMU x86_64 emulates ISA 8250 UARTs at the standard COM port I/O
        addresses.  COM1 (stdio) is the main console; COM2-COM4 are available
        for dedicated process output channels and appear inside the QNX
        guest as /dev/ser2 – /dev/ser4.
        """
        self._tmpdir = tempfile.mkdtemp(prefix="qemu_serial_")
        channels: list[SerialChannel] = []
        qemu_args: list[str] = []
        for i in range(self.NUM_EXTRA_SERIAL_PORTS):
            sock_path = os.path.join(self._tmpdir, f"serial_{i}.sock")
            # Guest device numbering: ser1 is the console, extras start at ser2
            guest_dev = f"/dev/ser{i + 2}"
            ch = SerialChannel(sock_path, guest_dev)
            channels.append(ch)
            qemu_args += ["-serial", f"unix:{sock_path},server=on,wait=off"]
        return channels, qemu_args

    def __init__(self, firmware):
        self._find_available_kvm_support()
        self._check_kvm_readable_when_necessary()

        channels, serial_args = self._create_serial_channels()

        qemu_options = ["-smp", "2",
                        "-m", "2G",
                        "-nographic",
                        "-serial", "stdio",
                        "-object", "rng-random,filename=/dev/urandom,id=rng0",
                        "-device", "virtio-rng-pci,rng=rng0",
                        f"{self._accelerator_support}",
                        "-cpu", "Cascadelake-Server-v5"
                        ] + serial_args

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

        # Connect to each extra serial socket created by QEMU.
        for ch in channels:
            ch.connect()
        self.channel_pool = SerialChannelPool(channels)

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
        self.channel_pool.close_all()
        self._socket.close()
        self._socket_file_descriptor.close()
        if hasattr(self, '_tmpdir') and os.path.isdir(self._tmpdir):
            shutil.rmtree(self._tmpdir, ignore_errors=True)
