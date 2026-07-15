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

"""Integration tests for service discovery offer and search."""


def client(target):
    args = []
    return target.wrap_exec("bin/client", args, cwd="/opt/ClientApp", wait_on_exit=True)


def service(target):
    args = ["--cycle-time", "250"]
    return target.wrap_exec("bin/service", args, cwd="/opt/ServiceApp")




def service_discovery_daemon(target, **kwargs):
    return target.wrap_exec(
        "bin/service_discovery_daemon_app",
        [],
        cwd="/opt/ServiceDiscoveryDaemonApp",
        **kwargs,
    )

def test_service_discovery_offer_and_search(target):
    """Test service discovery where service offers first, then client searches."""
    with service_discovery_daemon(target), service(target), client(target):
        pass
