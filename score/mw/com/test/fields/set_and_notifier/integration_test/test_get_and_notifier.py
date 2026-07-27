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

from test_fixture import consumer, provider, FieldScenario


def test_get_and_notifier_returns_initial_value(target):
    """Test that both GetNewSamples (notifier) and Get() return the value set by the provider via Update()."""
    with provider(target, FieldScenario.GET_AND_NOTIFIER, "get_and_notifier_mw_com_config.json"):
        with consumer(target, FieldScenario.GET_AND_NOTIFIER, "get_and_notifier_mw_com_config.json"):
            pass
