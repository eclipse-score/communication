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

"""Integration test for separate_reception_threads."""


def separate_reception_threads(target, **kwargs):
    args = ["-service_instance_manifest", "./etc/mw_com_config.json"]
    return target.wrap_exec(
        "bin/separate_reception_threads", args, cwd="/opt/separate_reception_threads", wait_on_exit=True, **kwargs
    )


def test_separate_reception_threads(target):
    """Test separate reception threads functionality.

    NOTE: The main application code for this test is disabled (#if 0) and just returns
    EXIT_SUCCESS immediately. This test verifies the stub runs without error.
    """
    with separate_reception_threads(target, wait_timeout=15):
        pass
