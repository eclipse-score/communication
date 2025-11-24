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
from quality.integration_testing.environments.qnx8_qemu.qemu_runner import QEMURunner
from quality.integration_testing.system_under_test import SystemUnderTest
from typing import Tuple
import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--qemu_image",
        action="store",
        required=True,
        help="Qemu image to run tests against.",
    )


class QemuUnderTest(SystemUnderTest):

    def __init__(self, qemu) -> None:
        self.__qemu = qemu

    def execute(self, command: str) -> Tuple[int, str]:
        return self.__qemu.console.run_sh_cmd_output(command)


@pytest.fixture()
def sut(request):
    qemu_image = request.config.getoption("qemu_image")
    with QEMURunner(qemu_image) as qemu:
        # We have to init the console once, after boot-up.
        qemu.console.run_cmd("echo")
        qemu_under_test = QemuUnderTest(qemu)
        yield qemu_under_test
