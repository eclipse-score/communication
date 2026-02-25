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

from quality.integration_testing.environments.qnx8_qemu.qemu_runner import QEMURunner
from quality.integration_testing.system_under_test import SystemUnderTest, Process
from typing import Tuple
import time
import random
import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--qemu_image",
        action="store",
        required=True,
        help="Qemu image to run tests against.",
    )

class QemuProcess(Process):
    def __init__(self, qemu, command: str, cwd = None, timeout: int = 60) -> None:
        self._qemu = qemu
        self._command = command
        self._timeout = timeout
        self._log_file = f"/tmp/{random.randint(10000, 99999)}.log"
        self._cwd = cwd

    def __enter__(self):
        self._pid = self._qemu.console.run_sh_cmd_async(f"/bin/sh -c 'cd {self._cwd}; {self._command} 2>&1 > {self._log_file}; echo $? > {self._log_file}.exit'")
        return self

    def wait_for_exit(self, timeout: int = 60) -> int:
        start_time = time.time()

        while time.time() - start_time < self._timeout:
            exit_code, output = self._qemu.console.run_sh_cmd_output(f"pidin -p {self._pid}")

            # If pidin fails or pid not in output, process has terminated
            if exit_code != 0 or str(self._pid) not in output:
                # Get exit code using wait command
                # The -n flag makes wait non-blocking (if supported)
                _, exit_code = self._qemu.console.run_sh_cmd_output(f"cat {self._log_file}.exit")
                try:
                    return int(exit_code.strip())
                except (ValueError, AttributeError):
                    # If wait fails (process already reaped), check log for errors
                    return -1

            time.sleep(0.1)  # Poll every 100ms

    def __exit__(self, exc_type, exc_val, exc_tb):
        _, output = self._qemu.console.run_sh_cmd_output(f"cat {self._log_file}")
        logging.info(f"{self._command} had output: {output}")

class QemuUnderTest(SystemUnderTest):

    def __init__(self, qemu) -> None:
        self.__qemu = qemu

    def execute(self, command: str) -> Tuple[int, str]:
        return self.__qemu.console.run_sh_cmd_output(command)

    def start_process(self, command: str, cwd: str = "") -> Process:
        return QemuProcess(self.__qemu, command, cwd = cwd)

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
