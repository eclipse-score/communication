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


def check_values_created_from_config(target, mode, **kwargs):
    args = ["-service_instance_manifest", "./etc/mw_com_config.json", "--mode", mode]
    return target.wrap_exec(
        "bin/check_values_created_from_config",
        args,
        cwd="/opt/check_values_created_from_config",
        wait_on_exit=True,
        **kwargs,
    )


def test_check_values_created_from_config(target):
    """Test that config values are correctly created and exchanged."""
    with (
        check_values_created_from_config(target, mode="send", wait_timeout=15),
        check_values_created_from_config(target, mode="recv", wait_timeout=15),
    ):
        pass
