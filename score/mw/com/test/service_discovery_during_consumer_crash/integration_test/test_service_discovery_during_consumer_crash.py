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

"""Integration test for service_discovery_during_consumer_crash."""


def test_service_discovery_during_consumer_crash(sut):
    """Test service discovery behavior when consumer crashes and restarts."""
    with sut.start_process(
        "./bin/service_discovery_during_consumer_crash_application -service_instance_manifest ./etc/mw_com_config.json -t 1",
        cwd="/opt/service_discovery_during_consumer_crash",
    ) as process:
        assert process.wait_for_exit() == 0
