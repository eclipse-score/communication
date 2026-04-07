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

def test_multiple_proxies(sut):
    with sut.start_process("./bin/stop_offer_during_call",
                           cwd="/opt/StopOfferDuringCallApp/") as process:
        assert process.wait_for_exit() == 0
