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


def test_field_setter_and_getter_different_process(target):
    with provider(target, FieldScenario.SETTER_AND_GETTER, "mw_com_config.json"):
        with consumer(target, FieldScenario.SETTER_AND_GETTER, "mw_com_config.json"):
            pass
