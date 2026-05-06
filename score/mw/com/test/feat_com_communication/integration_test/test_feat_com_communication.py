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

"""Integration test to verify primary feat__com_communication communication framework robustness."""

import time

def test_feat_com_communication(sut):
    """Verifies that the proxy receives IPC events from the skeleton concurrently over standard configurations."""
    with sut.start_process("./bin/feat_com_communication_app --mode send -t 50 -n 20", cwd="/opt/feat_com_communication/") as sender_process:
        with sut.start_process("./bin/feat_com_communication_app --mode recv -n 15", cwd="/opt/feat_com_communication/") as receiver_process:
            assert receiver_process.wait_for_exit(timeout=60) == 0

        assert sender_process.wait_for_exit(timeout=60) == 0

def test_feat_com_broadcasting_multiple_receivers(sut):
    """Verifies that multiple proxies can concurrently receive data from a single skeleton."""
    with sut.start_process("./bin/feat_com_communication_app --mode send -t 50 -n 400", cwd="/opt/feat_com_communication/") as sender_process:
        
        with sut.start_process("./bin/feat_com_communication_app --mode recv -n 15 --service_instance_manifest etc/mw_com_config_recv1.json", cwd="/opt/feat_com_communication/") as recv1:
            time.sleep(1.0)
            with sut.start_process("./bin/feat_com_communication_app --mode recv -n 15 --service_instance_manifest etc/mw_com_config_recv2.json", cwd="/opt/feat_com_communication/") as recv2:
                time.sleep(1.0)
                with sut.start_process("./bin/feat_com_communication_app --mode recv -n 15 --service_instance_manifest etc/mw_com_config_recv3.json", cwd="/opt/feat_com_communication/") as recv3:
                    
                    
                    assert recv1.wait_for_exit(timeout=90) == 0
                    assert recv2.wait_for_exit(timeout=90) == 0
                    assert recv3.wait_for_exit(timeout=90) == 0

        assert sender_process.wait_for_exit(timeout=90) == 0

def test_feat_com_late_subscriber_joins(sut):
    """Verifies that a proxy can successfully discover and receive data after the skeleton has already started publishing."""
    with sut.start_process("./bin/feat_com_communication_app --mode send -t 50 -n 40", cwd="/opt/feat_com_communication/") as sender_process:
        
        time.sleep(1.0)
        with sut.start_process("./bin/feat_com_communication_app --mode recv -n 15", cwd="/opt/feat_com_communication/") as receiver_process:
            assert receiver_process.wait_for_exit(timeout=60) == 0
            
        assert sender_process.wait_for_exit(timeout=60) == 0
