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

"""Integration test for subscribe_handler."""


def test_subscribe_handler(sut):
    """Test subscription state callback handling when proxy is destroyed before callback.
    
    This test ensures that if a proxy event is destroyed before a subscription state
    callback is called, the subscription is revoked and the program doesn't crash.
    """
    with sut.start_process("./bin/subscribe_handler -m send", cwd="/opt/subscribe_handler/") as sender:
        with sut.start_process("./bin/subscribe_handler -m recv", cwd="/opt/subscribe_handler/") as receiver:
            # Receiver should complete first (it destroys proxy before callback)
            assert receiver.wait_for_exit() == 0
            # Sender should also complete successfully
            assert sender.wait_for_exit() == 0
