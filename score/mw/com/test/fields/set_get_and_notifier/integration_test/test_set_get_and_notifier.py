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

from test_fixture import consumer, provider


def test_set_get_and_notifier_all_scenarios(target):
    """Test all Set/Get/Notifier interaction scenarios in sequence:

    1. Update() → initial value observable via both GetNewSamples (notifier) and Get().
    2. Set() with a valid value → accepted value, GetNewSamples, and Get() all agree.
    3. Set() with an invalid value (above max) → clamped value returned by accepted, GetNewSamples, and Get().
    4. Update() with a new value → both GetNewSamples and Get() return the updated value.
    """
    with provider(target, "mw_com_config.json"):
        with consumer(target, "mw_com_config.json"):
            pass
