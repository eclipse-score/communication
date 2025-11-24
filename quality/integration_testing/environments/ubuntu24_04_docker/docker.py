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
import subprocess
import docker as pypi_docker
from quality.integration_testing.system_under_test import SystemUnderTest
from typing import Tuple
import pytest


logger = logging.getLogger(__name__)


def pytest_addoption(parser):
    parser.addoption(
        "--docker-image",
        action="store",
        required=True,
        help="Docker image to run tests against.",
    )
    parser.addoption(
        "--docker-image-bootstrap",
        action="store",
        required=False,
        help="Docker image bootstrap command, that will be executed before referencing the container.",
    )

class DockerUnderTest(SystemUnderTest):

    def __init__(self, container) -> None:
        self.__container = container
    def execute(self, command: str) -> Tuple[int, str]:
        exit_code, bytes = self.__container.exec_run(command)
        return (exit_code, bytes.decode("utf-8"))

@pytest.fixture()
def sut(request):
    docker_image_bootstrap = request.config.getoption("docker_image_bootstrap")
    if docker_image_bootstrap:
        logger.info(f"Executing custom image bootstrap command: {docker_image_bootstrap}")
        subprocess.run([docker_image_bootstrap], check=True)

    docker_image = request.config.getoption("docker_image")
    client = pypi_docker.from_env()
    container = client.containers.run(docker_image, "sleep infinity", detach=True, auto_remove=True, init=True)
    docker_under_test = DockerUnderTest(container)
    yield docker_under_test
    container.stop(timeout=1)
