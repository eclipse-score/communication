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

"""Integration test for service_discovery_during_provider_crash."""

NUMBER_RESTART_CYCLES = 1
TEST_DIRECTORY_NAME = "service_discovery_during_provider_crash"


def controller_process(target, number_restart_cycles, **kwargs):
    args = [
        "--iterations",
        f"{number_restart_cycles}",
        "--service_instance_manifest",
        "etc/mw_com_config.json",
    ]
    return target.wrap_exec(
        f"bin/{TEST_DIRECTORY_NAME}_application", args, cwd=f"/opt/{TEST_DIRECTORY_NAME}", wait_on_exit=True, **kwargs
    )


def test_service_discovery_during_provider_crash(target):
    """Test service discovery behavior when provider crashes and restarts."""
    with controller_process(target, NUMBER_RESTART_CYCLES):
        pass
