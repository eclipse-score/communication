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


def test_set_and_get_valid_clamped_and_update(target):
    """Test all Set/Get interaction scenarios in sequence:

    1. Get() returns the initial value set by the skeleton's Update().
    2. Set() with a valid value (within range) → Get() returns that value.
    3. Set() with an invalid value (above max) → Set accepted value and Get() both return the clamped max.
    4. After the skeleton calls Update() with a new value, Get() returns the updated value.
    """
    with provider(target, "mw_com_config.json"):
        with consumer(target, "mw_com_config.json"):
            pass
