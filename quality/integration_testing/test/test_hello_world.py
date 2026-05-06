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
def test_hello_world_via_shell(target):
    exit_code, output = target.execute("echo Hello World!")
    assert 0 == exit_code
    assert b"Hello World!" in output


def test_hello_world_via_binary_execution(target):
    exit_code, output = target.execute("/example-app")
    assert 0 == exit_code
    assert b"Hello!" in output


def test_hello_world_as_process(target):
    with target.wrap_exec("/example-app2"):
        with target.wrap_exec("/example-app", wait_on_exit=True):
            pass
