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


def subscribe_handler(target, mode):
    args = ["--mode", mode]
    return target.wrap_exec(
        "bin/subscribe_handler", args, cwd="/opt/subscribe_handler", wait_on_exit=True, wait_timeout=15
    )


def test_subscribe_handler(target):
    """Test subscription state callback handling when proxy is destroyed before callback.

    This test ensures that if a proxy event is destroyed before a subscription state
    callback is called, the subscription is revoked and the program doesn't crash.
    """
    with subscribe_handler(target, "send"), subscribe_handler(target, "recv"):
        pass
