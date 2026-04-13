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

from partial_restart_methods_provider_test_fixture import provider_restart

NUMBER_RESTART_CYCLES = 10
WITH_PROXY = 1
KILL_PROVIDER = 0


def test_partial_restart_methods_provider_graceful_no_proxy(sut):
    """Test that methods work after provider graceful restart with proxy connected."""
    provider_restart(sut, NUMBER_RESTART_CYCLES, WITH_PROXY, KILL_PROVIDER)
