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

"""Integration tests for service discovery search and offer."""


def client(target, **kwargs):
    args = []
    return target.wrap_exec("bin/client", args, cwd="/opt/ClientApp", wait_on_exit=True, **kwargs)


def service(target, **kwargs):
    args = ["--cycle-time", "250"]
    return target.wrap_exec("bin/service", args, cwd="/opt/ServiceApp", **kwargs)


def test_service_discovery_search_and_offer(target):
    """Test service discovery where client searches first, then service offers."""
    with service(target):
        with client(target):
            pass
