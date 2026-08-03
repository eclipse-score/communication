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
from enum import Enum


class FieldScenario(str, Enum):
    NOTIFIER = "notifier"
    SET_AND_NOTIFIER = "set_and_notifier"
    GET = "get"
    GET_AND_NOTIFIER = "get_and_notifier"
    SET_AND_GET = "set_and_get"
    SET_GET_AND_NOTIFIER = "set_get_and_notifier"


def consumer(target, scenario, config, **kwargs):
    args = ["--mode", scenario.value, "--service-instance-manifest", f"./etc/{config}"]
    return target.wrap_exec("bin/consumer", args, cwd="/opt/MainConsumerApp", wait_on_exit=True, **kwargs)


def provider(target, scenario, config, **kwargs):
    args = ["--mode", scenario.value, "--service-instance-manifest", f"./etc/{config}"]
    return target.wrap_exec("bin/provider", args, cwd="/opt/MainProviderApp", **kwargs)


def consumer_and_provider(target, scenario, config, **kwargs):
    args = ["--mode", scenario.value, "--service-instance-manifest", f"./etc/{config}"]
    return target.wrap_exec(
        "bin/consumer_and_provider",
        args,
        cwd="/opt/MainConsumerAndProviderApp",
        wait_on_exit=True,
        **kwargs,
    )
