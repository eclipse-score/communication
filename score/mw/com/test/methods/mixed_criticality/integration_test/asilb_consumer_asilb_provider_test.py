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
from test_fixture import Criticality, consumer, provider


"""
Test all four method types (InArg and Return, InArg only, Return only, and no InArg or Return) with ASIL B criticality for both consumer and provider.
Verifies that method calls are sent from consumer to provider with the correct values.
Verifies that the provider executes the correct handler on the provided values. 
Verifies that the consumer receives the correct return values.
"""
def test_mixed_criticality_consumer_provider(target):
    with provider(target, Criticality.ASILB):
        with consumer(target, consumer_criticality=Criticality.ASILB):
            pass
