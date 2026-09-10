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

"""
The goal of this test is to demonstrate a failiure mode.

An unwanted server which is running at a time when it shouldn't and has the same service_identifier as the intended
server, may intercept the communication from the client and react to it in an arbitrary way.
"""

MESSAGE_CHAR = "1"


def client(target, **kwargs):
    args = [MESSAGE_CHAR]
    return target.wrap_exec("bin/client", args, cwd="/opt/ClientApp", wait_on_exit=True, **kwargs)


def server(target, **kwargs):
    expecting_message = "0"
    server_id = "Server"
    args = [MESSAGE_CHAR, expecting_message, server_id]
    return target.wrap_exec("bin/server", args, cwd="/opt/ServerApp", wait_on_exit=True, **kwargs)


def unwanted_server(target, **kwargs):
    expecting_message = "1"
    server_id = "Unwanted Server"
    args = [MESSAGE_CHAR, expecting_message, server_id]
    return target.wrap_exec("bin/server", args, cwd="/opt/ServerApp", wait_on_exit=True, **kwargs)


# gToDo: this test needs to succeed if the normal sever fails to connect
def test_main(target):
    with client(target), unwanted_server(target), server(target):
        pass
