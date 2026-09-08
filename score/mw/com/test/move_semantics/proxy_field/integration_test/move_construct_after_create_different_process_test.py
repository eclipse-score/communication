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
from test_fixture import consumer, provider, ProxyMoveScenario


def test_move_construct_after_create_different_process(target):
    with provider(target, ProxyMoveScenario.MOVE_CONSTRUCT_AFTER_CREATE):
        with consumer(target, ProxyMoveScenario.MOVE_CONSTRUCT_AFTER_CREATE):
            pass
