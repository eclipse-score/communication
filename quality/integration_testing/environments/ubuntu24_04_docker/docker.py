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
from quality.integration_testing.system_under_test import SystemUnderTest, Process
from typing import Tuple
import time
import threading
import pytest

logger = logging.getLogger(__name__)
logging.getLogger("urllib3").setLevel(logging.WARNING)
logging.getLogger("docker").setLevel(logging.WARNING)


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


class DockerProcess(Process):
    def __init__(self, container, binary_path: str, cwd: str = "", timeout: int = 60) -> None:
        self._container = container
        self._binary_path = binary_path
        self._timeout = timeout
        self._cwd = cwd
        self._stop_flag = threading.Event()

    def __enter__(self):
        exec_id = self._container.client.api.exec_create(
            self._container.id,
            f"{self._binary_path}",
            workdir=self._cwd,
            stdout=True,
            stderr=True
        )['Id']

        def stream_output():
            output = self._container.client.api.exec_start(exec_id, stream=True)
            process_logger = logging.getLogger(self._binary_path)
            for chunk in output:
                process_logger.info(chunk.decode('utf-8'))
                if self._stop_flag.is_set():
                    break

        self._thread = threading.Thread(target=stream_output, daemon=True)
        self._thread.start()

        self._exec_id = exec_id

        # Wait for process to actually start
        start_time = time.time()
        while time.time() - start_time < 5:  # 5 second timeout
            exec_info = self._container.client.api.exec_inspect(self._exec_id)
            if exec_info['Running']:
                return self
            time.sleep(0.05)
        
        return self

    def wait_for_exit(self, timeout: int = 60) -> int:
        if self._exec_id:
            start_time = time.time()
            while time.time() - start_time < self._timeout:
                exec_info = self._container.client.api.exec_inspect(self._exec_id)
                print(exec_info)
                if not exec_info['Running']:
                    self._stop_flag.set()
                    self._thread.join(timeout=timeout)

                if not exec_info['Running'] and exec_info['ExitCode'] is not None:
                    return int(exec_info['ExitCode'])
                time.sleep(0.1)
        return -1  # Timeout

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._stop_flag.set()


class DockerUnderTest(SystemUnderTest):

    def __init__(self, container) -> None:
        self.__container = container

    def execute(self, command: str) -> Tuple[int, str]:
        exit_code, bytes = self.__container.exec_run(command)
        return (exit_code, bytes.decode("utf-8"))

    def start_process(self, command: str, cwd: str = "") -> Process:
        return DockerProcess(self.__container, command, cwd=cwd)


@pytest.fixture()
def sut(request):
    docker_image_bootstrap = request.config.getoption("docker_image_bootstrap")
    if docker_image_bootstrap:
        logger.info(f"Executing custom image bootstrap command: {docker_image_bootstrap}")
        subprocess.run([docker_image_bootstrap], check=True)

    docker_image = request.config.getoption("docker_image")
    client = pypi_docker.from_env()
    # Increase shared memory size to 1GB to support tests with large shared memory requirements
    # (default is 64MB which is insufficient for tests like receive_handler_usage that need 218MB)
    container = client.containers.run(docker_image, "sleep infinity", detach=True, auto_remove=True, init=True, shm_size='1g')
    docker_under_test = DockerUnderTest(container)
    yield docker_under_test
    container.stop(timeout=1)
