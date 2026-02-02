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

"""Integration test for provider_restart_max_subscribers."""


def test_provider_restart_max_subscribers(sut):
    """Test provider restart with maximum subscribers functionality."""
    with sut.start_process(
        "./bin/provider_restart_max_subscribers_application -service_instance_manifest ./etc/mw_com_config.json -t 1",
        cwd="/opt/provider_restart_max_subscribers",
    ) as process:
        assert process.wait_for_exit() == 0
