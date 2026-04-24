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

"""Integration test for proxy_restart_shall_not_affect_other_proxies (graceful)."""

from proxy_restart_shall_not_affect_other_proxies_test_fixture import proxy_restart_shall_not_affect_other_proxies

# Test configuration
NUMBER_OF_CONSUMER_RESTARTS = 20
KILL_CONSUMER = 0


def test_proxy_restart_shall_not_affect_other_proxies_graceful(target):
    """Test that graceful proxy restart does not affect other proxies."""
    with proxy_restart_shall_not_affect_other_proxies(target, NUMBER_OF_CONSUMER_RESTARTS, KILL_CONSUMER):
        pass
