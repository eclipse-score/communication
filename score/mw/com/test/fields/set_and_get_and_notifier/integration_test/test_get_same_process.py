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

from test_fixture import consumer_and_provider, FieldScenario


def test_field_get_same_process(target):
    """Test get field flow when consumer and provider run in the same process."""
    with consumer_and_provider(target, FieldScenario.GET, "mw_com_config.json"):
        pass
