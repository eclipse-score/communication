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


def test_field_set_and_notifier_value(target):
    """Test field set exchange and accepted value propagation between provider and consumer."""
    with provider(target, "set_and_notifier", "mw_com_config.json"):
        with consumer(target, "set_and_notifier", "mw_com_config.json"):
            pass


# TODO: Add a dedicated get scenario test once getter-enabled field mode is available.
