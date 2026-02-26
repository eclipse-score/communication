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

"""Integration test for proxy_restart_shall_not_affect_other_proxies."""


def test_proxy_restart_shall_not_affect_other_proxies(sut):
    """Test proxy restart shall not affect other proxies functionality."""
    with sut.start_process(
        "./bin/proxy_restart_shall_not_affect_other_proxies --number-consumer-restarts 1 --kill false",
        cwd="/opt/proxy_restart_shall_not_affect_other_proxies",
    ) as process:
        assert process.wait_for_exit() == 0
