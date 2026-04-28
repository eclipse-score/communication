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

"""Integration test for provider_restart_max_subscribers (with proxy connected)."""

from provider_restart_max_subscribers_test_fixture import provider_restart_max_subscribers

# Test configuration
IS_PROXY_CONNECTED_DURING_RESTART = 1


def test_provider_restart_max_subscribers_with_proxy(target):
    """Test provider restart with maximum subscribers and proxy connected during restart."""
    with provider_restart_max_subscribers(target, IS_PROXY_CONNECTED_DURING_RESTART):
        pass
