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

"""Integration test for reserving_skeleton_slots."""


def test_reserving_skeleton_slots_passing(sut):
    """Test that skeleton slots can be reserved correctly when numbers match."""
    with sut.start_process(
        "./bin/reserving_skeleton_slots -service_instance_manifest ./etc/mw_com_config.json -m passing",
        cwd="/opt/reserving_skeleton_slots/"
    ) as process:
        assert process.wait_for_exit() == 0


def test_reserving_skeleton_slots_failing_extra_slots(sut):
    """Test that requesting more skeleton slots than configured fails as expected."""
    with sut.start_process(
        "./bin/reserving_skeleton_slots -service_instance_manifest ./etc/mw_com_config.json -m failing_extra_slots",
        cwd="/opt/reserving_skeleton_slots/"
    ) as process:
        assert process.wait_for_exit() == 0


def test_reserving_skeleton_slots_failing_less_slots(sut):
    """Test that requesting fewer skeleton slots than needed fails as expected."""
    with sut.start_process(
        "./bin/reserving_skeleton_slots -service_instance_manifest ./etc/mw_com_config.json -m failing_less_slots",
        cwd="/opt/reserving_skeleton_slots/"
    ) as process:
        assert process.wait_for_exit() == 0
