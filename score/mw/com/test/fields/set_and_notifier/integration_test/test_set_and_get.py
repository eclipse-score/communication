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


def test_set_and_get_valid_clamped_and_update(target):
    """Test Set/Get interactions: valid set, clamped set, and provider Update() all verifiable via Get()."""
    with provider(target, FieldScenario.SET_AND_GET, "set_and_get_mw_com_config.json"):
        with consumer(target, FieldScenario.SET_AND_GET, "set_and_get_mw_com_config.json"):
            pass
