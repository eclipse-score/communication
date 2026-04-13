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
import logging
import threading

from quality.integration_testing.environments.qnx8_qemu.qemu_runner import QEMURunner
from quality.integration_testing.system_under_test import SystemUnderTest, Process
from typing import Tuple
import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--qemu_image",
        action="store",
        required=True,
        help="Qemu image to run tests against.",
    )

class QemuProcess(Process):
    """Runs a command inside the QEMU guest with live output streaming.

    Each process is assigned a dedicated QEMU serial port channel so its
    output never intermixes with other processes or the main console.
    The host reads from the channel's Unix socket in a background thread,
    providing real-time log output.

    Completion is signaled entirely through the serial channel: after the
    command finishes, a unique sentinel line containing the exit code is
    written to the same serial device.  The background stream-reader thread
    detects the sentinel, stores the exit code, and sets an event so that
    ``wait_for_exit`` returns.

    The main serial console is only used to launch the background command
    and retrieve its PID. This is done in a blocking manner to ensure proper
    synchronization.
    """

    _EXIT_SENTINEL = "___QEMU_HARNESS_EXIT_CODE___"

    def __init__(self, qemu, channel_pool, command: str, cwd=None, timeout: int = 60) -> None:
        self._qemu = qemu
        self._channel_pool = channel_pool
        self._command = command
        self._timeout = timeout
        self._cwd = cwd
        self._channel = None
        self._stop_event = threading.Event()
        self._done_event = threading.Event()
        self._pid = -1
        self._exit_code = -1
        self._stream_thread = None
        self._output_lines = []
        self._logger = logging.getLogger(f"QemuProcess[{command}]")

    def __enter__(self):
        self._channel = self._channel_pool.acquire()
        if self._channel is None:
            raise RuntimeError(
                "No serial channels available. Cannot run more concurrent "
                "processes than NUM_EXTRA_SERIAL_PORTS configured in QEMURunner."
            )

        cwd_prefix = f"cd {self._cwd}; " if self._cwd else ""
        guest_dev = self._channel.guest_device

        # Build a shell command that:
        #  1. Redirects stdout+stderr to the dedicated serial device
        #  2. Traps SIGINT to forward it to the child process, allowing early termination with sentinel line
        #  3. After the command exits, echoes a sentinel line carrying the
        #     exit code to the same serial device
        # The command is launched as a background job via the main console.
        shell_cmd = (
            "/bin/sh -c '"
            f"{cwd_prefix}"
            f"{self._command} > {guest_dev} 2>&1 &"
            "CHILD_PID=$!; "  # Capture the PID of the background job
            "trap \"kill -s SIGINT $CHILD_PID\" INT; "
            "wait $CHILD_PID; "  # Wait for the command to finish
            f"echo \"\r\n{self._EXIT_SENTINEL}=$?\" > {guest_dev}"
            "'"
        )

        self._pid = self._qemu.console.run_sh_cmd_async(shell_cmd)

        # Sync: Mandatory step to wait for the shell to process the background launch.
        # We use this moment to also inform the user about launched command.
        self._logger.info(f"Launched command {cwd_prefix}{self._command} with PID {self._pid} and output redirected to {guest_dev}")

        # Start reading from the channel's Unix socket in a background thread
        self._stream_thread = threading.Thread(
            target=self._stream_output,
            name=f"stream-{self._command}",
            daemon=True,
        )
        self._stream_thread.start()

        return self

    def _stream_output(self):
        """Read lines from the serial channel socket and log them live.

        When the exit-sentinel line is detected the exit code is stored and
        the done-event is set so that ``wait_for_exit`` can return
        immediately.
        """
        try:
            sock_file = self._channel.makefile()
            while not self._stop_event.is_set():
                line = sock_file.readline()
                if not line:
                    break
                decoded = line.decode("utf-8", errors="replace").rstrip("\r\n")
                if not decoded:
                    continue

                # Strip leading serial flow-control characters (e.g. XON \x11,
                # XOFF \x13) that the QNX serial driver may inject.
                stripped = decoded.lstrip("\x00\x11\x13")

                # Check for the exit-sentinel written after the command
                if stripped.startswith(self._EXIT_SENTINEL + "="):
                    try:
                        self._exit_code = int(
                            stripped.split("=", 1)[1].strip()
                        )
                    except (ValueError, IndexError):
                        self._exit_code = -1
                    self._done_event.set()
                    break

                self._output_lines.append(decoded)
                self._logger.info(decoded)
        except Exception as e:
            if not self._stop_event.is_set():
                self._logger.warning(f"Stream read error: {e}")

    def wait_for_exit(self, timeout: int = 60) -> int:
        effective_timeout = timeout if timeout else self._timeout

        if self._done_event.wait(timeout=effective_timeout):
            return self._exit_code

        self._logger.error(f"Timed out waiting for process {self._pid} to finish")
        return -1

    def kill(self):
        self._qemu.console.run_sh_cmd_output(f"kill -s SIGINT {self._pid}")
        self._logger.info(f"Killed {self._pid} - waiting for process to finish")
        self.wait_for_exit()

    def __exit__(self, exc_type, exc_val, exc_tb):
        # If the process hasn't finished yet, wait for it so the sentinel
        # is properly consumed before we tear down the stream thread.
        if not self._done_event.is_set():
            self.kill()

        self._stop_event.set()
        if self._stream_thread:
            self._stream_thread.join(timeout=5)

        # Release the channel back to the pool
        if self._channel:
            self._channel_pool.release(self._channel)
            self._channel = None


class QemuUnderTest(SystemUnderTest):

    def __init__(self, qemu) -> None:
        self.__qemu = qemu
        self.__channel_pool = qemu.channel_pool

    def execute(self, command: str) -> Tuple[int, str]:
        return self.__qemu.console.run_sh_cmd_output(command)

    def start_process(self, command: str, cwd: str = "") -> Process:
        return QemuProcess(self.__qemu, self.__channel_pool, command, cwd=cwd)

@pytest.fixture()
def sut(request):
    qemu_image = request.config.getoption("qemu_image")
    with QEMURunner(qemu_image) as qemu:
        found = False
        while not found:
            line = qemu.console.readline(block=True, timeout=10)
            if line and "Welcome to QNX" in line:
                found = True
        # We have to init the console once, after boot-up.
        qemu.console.run_cmd("\n\n\n\n\n\n\n")
        qemu.console.run_sh_cmd_output("echo")
        exit_code, _ = qemu.console.run_sh_cmd_output("set +m")  # Disable job control messages
        assert exit_code == 0
        qemu.console.clear_history()
        qemu_under_test = QemuUnderTest(qemu)
        yield qemu_under_test
