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


def test_update_is_observable_via_both_get_and_notifier(target):
    """Test that the value set by the skeleton's Update() is observable via both
    GetNewSamples (notifier subscription) and Get() on the same field."""
    with provider(target, "mw_com_config.json"):
        with consumer(target, "mw_com_config.json"):
            pass
