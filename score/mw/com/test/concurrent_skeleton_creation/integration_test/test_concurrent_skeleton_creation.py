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

"""Integration test for concurrent_skeleton_creation."""


def test_concurrent_skeleton_creation(sut):
    """Test concurrent skeleton creation and offer behavior."""
    with sut.start_process(
        "./bin/concurrent_skeleton_creation --service_instance_manifest ./etc/mw_com_config.json",
        cwd="/opt/concurrent_skeleton_creation",
    ) as process:
        assert process.wait_for_exit() == 0
