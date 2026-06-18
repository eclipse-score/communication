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


class SkeletonMoveScenario(IntEnum):
    MOVE_CONSTRUCT_NOT_OFFERED = 0
    MOVE_CONSTRUCT_OFFERED = 1
    MOVE_ASSIGN_NOT_OFFERED = 2
    MOVE_ASSIGN_OFFERED = 3


def consumer_and_provider(target, scenario, number_of_samples_to_send_per_offer, **kwargs):
    args = ["--scenario", str(int(scenario)), "--number-of-samples-to-send", str(number_of_samples_to_send_per_offer), "--service-instance-manifest", f"./etc/mw_com_config.json"]
    return target.wrap_exec(
        "bin/main_consumer_and_provider", args, cwd="/opt/MainConsumerAndProviderApp", wait_on_exit=True, **kwargs
    )

def consumer(target, scenario, number_of_samples_to_send_per_offer, **kwargs):
    args = ["--scenario", str(int(scenario)), "--number-of-samples-to-send", str(number_of_samples_to_send_per_offer), "--service-instance-manifest", f"./etc/mw_com_config.json"]
    return target.wrap_exec(
        "bin/main_consumer", args, cwd="/opt/MainConsumerApp", wait_on_exit=True, **kwargs
    )

def provider(target, scenario, number_of_samples_to_send_per_offer, **kwargs):
    args = ["--scenario", str(int(scenario)), "--number-of-samples-to-send", str(number_of_samples_to_send_per_offer), "--service-instance-manifest", f"./etc/mw_com_config.json"]
    return target.wrap_exec(
        "bin/main_provider", args, cwd="/opt/MainProviderApp", wait_on_exit=True, **kwargs
    )

