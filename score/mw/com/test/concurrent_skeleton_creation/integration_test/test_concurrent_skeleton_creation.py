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


def concurrent_skeleton_creation(target, **kwargs):
    args = [
        "--service_instance_manifest",
        "etc/mw_com_config.json"
    ]
    return target.wrap_exec(
        "bin/concurrent_skeleton_creation", args, cwd="/opt/concurrent_skeleton_creation", wait_on_exit=True, **kwargs
    )


def test_concurrent_skeleton_creation(target):
    """Test concurrent skeleton creation and offer behavior."""
    with concurrent_skeleton_creation(target, wait_timeout=30):
        pass
