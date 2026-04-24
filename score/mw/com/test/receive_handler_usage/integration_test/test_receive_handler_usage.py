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

"""Integration tests for receive handler usage."""


def receive_handler_usage(target, **kwargs):
    args = []
    return target.wrap_exec(
        "bin/receive_handler_usage", args, cwd="/opt/receive_handler_usage", wait_on_exit=True, **kwargs
    )


def test_receive_handler_usage(target):
    """Test that mw::com APIs can be called from within EventReceiveHandler without deadlocks."""
    with receive_handler_usage(target, wait_timeout=15):
        pass
