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
a misconfigured client, which is running at a time when it shouldn't may cause the server to receive a message and react
to it, instead of reacting to the message of the correctly configured client.
"""


def client(target, **kwargs):
    message_char = "a"
    args = [message_char]
    return target.wrap_exec("bin/client", args, cwd="/opt/ClientApp", wait_on_exit=True, **kwargs)


def misconfigured_client(target, **kwargs):
    """
    misconfigured_client in this case referres to the fact that the client runs when it should not be running, and not
    to the message it sends.
    for the purposes of this thest this client will send exactly the message that the server is expecting (but not from
    this client)
    """
    message_char = "b"
    args = [message_char]
    return target.wrap_exec("bin/client", args, cwd="/opt/ClientApp", wait_on_exit=True, **kwargs)


def server(target, **kwargs):
    expected_message_char = "b"
    expecting_message = "1"
    server_id = "Server"
    args = [expected_message_char, expecting_message, server_id]
    return target.wrap_exec("bin/server", args, cwd="/opt/ServerApp", wait_on_exit=True, **kwargs)


def test_main(target):
    # this test confirms the failiure mode that a misconfigured client can send communication before the intended client
    with server(target, wait_timeout=120), misconfigured_client(target, wait_timeout=120), client(target, wait_timeout=120):
        pass
