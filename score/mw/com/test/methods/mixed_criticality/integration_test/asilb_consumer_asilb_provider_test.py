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
from test_fixture import Criticality, call_consumer_and_provider


def test_mixed_criticality_consumer_provider(sut):
    call_consumer_and_provider(
        sut, Criticality.ASILB, Criticality.ASILB, Criticality.ASILB)
