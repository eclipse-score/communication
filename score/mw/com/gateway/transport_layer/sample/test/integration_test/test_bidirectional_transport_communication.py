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

# Integration test that verifies behavior of BidirectionalTransport.
# App2 sends two messages to app1 and will handle one response message from app1.
def test_bidirectional_transport_communication(target):
    with target.wrap_exec("/app1", wait_on_exit=True):
        with target.wrap_exec("/app2", wait_on_exit=True):
            pass