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


def test_check_values_created_from_config(sut):
    """Test that config values are correctly created and exchanged."""
    with sut.start_process("./bin/check_values_created_from_config -service_instance_manifest ./etc/mw_com_config.json --mode send", cwd="/opt/check_values_created_from_config/") as sender_process:
        with sut.start_process("./bin/check_values_created_from_config -service_instance_manifest ./etc/mw_com_config.json --mode recv", cwd="/opt/check_values_created_from_config/") as receiver_process:
            assert receiver_process.wait_for_exit() == 0
