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
def test_hello_world_via_shell(sut):
    exit_code, output = sut.execute("echo Hello World!")
    assert 0 == exit_code
    assert "Hello World!" in output

def test_hello_world_via_binary_execution(sut):
    exit_code, output = sut.execute("/example-app")
    assert 0 == exit_code
    assert "Hello!" in output

def test_hello_world_as_process(sut):
    with sut.start_process('/example-app2') as example_2:
        with sut.start_process("/example-app") as example:
            example.wait_for_exit() == 0
