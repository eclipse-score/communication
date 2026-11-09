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
from test_fixture import SkeletonReofferScenario, consumer, provider


def test_reoffer_after_proxy_disconnected_different_processes(target):
    with (
        provider(target, SkeletonReofferScenario.PROXY_DISCONNECTED_BEFORE_REOFFER),
        consumer(target, SkeletonReofferScenario.PROXY_DISCONNECTED_BEFORE_REOFFER),
    ):
        pass
