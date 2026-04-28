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


def reserving_skeleton_slots(target, mode, **kwargs):
    args = [
        "-service_instance_manifest",
        "etc/mw_com_config.json",
        "--mode",
        mode,
    ]
    return target.wrap_exec(
        "bin/reserving_skeleton_slots", args, cwd="/opt/reserving_skeleton_slots", wait_on_exit=True, **kwargs
    )


def test_reserving_skeleton_slots(target):
    """Test skeleton slot reservation with passing and failing configurations."""
    # Test that skeleton slots can be reserved correctly when numbers match
    with reserving_skeleton_slots(target, "passing", wait_timeout=15):
        pass

    # Test that requesting more skeleton slots than configured fails as expected
    with reserving_skeleton_slots(target, "failing_extra_slots", wait_timeout=15):
        pass

    # Test that requesting fewer skeleton slots than needed fails as expected
    with reserving_skeleton_slots(target, "failing_less_slots", wait_timeout=15):
        pass
