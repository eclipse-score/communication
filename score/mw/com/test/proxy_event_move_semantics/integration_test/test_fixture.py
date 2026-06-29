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
from enum import IntEnum


class ProxyMoveScenario(IntEnum):
    MOVE_CONSTRUCT_BEFORE_SUBSCRIBE = 0


def consumer_and_provider(target, scenario, **kwargs):
    args = ["--scenario", str(int(scenario)), "--service-instance-manifest", "./etc/mw_com_config.json"]
    return target.wrap_exec(
        "bin/main_consumer_and_provider", args, cwd="/opt/MainConsumerAndProviderApp", wait_on_exit=True, **kwargs
    )
