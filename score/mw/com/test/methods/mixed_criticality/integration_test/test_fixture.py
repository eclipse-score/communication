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


class Criticality(Enum):
    ASILB = "asil_b"
    QM = "qm"


def consumer(target, consumer_criticality: Criticality, consumer_expects_criticality: Criticality, **kwargs):
    # consumer app may be asil b but consume asil qm provider thus two separate
    # Criticality values need to be specified here
    consumer_config_name = (
        f"consumer_{consumer_criticality.value}_to_{consumer_expects_criticality.value}_mw_com_config.json")

    args = ["--service_instance_manifest", f"./etc/{consumer_config_name}"]
    return target.wrap_exec("bin/consumer_main", args, cwd="/opt/ConsumerApp", wait_on_exit=True, **kwargs)


def provider(target, provider_criticality: Criticality, **kwargs):
    # For a provider the global app criticality and the
    # offered criticality is always the same
    provider_config_name = (
        f"provider_{provider_criticality.value}_mw_com_config.json")

    args = ["--service_instance_manifest", f"./etc/{provider_config_name}"]
    return target.wrap_exec("bin/provider_main", args, cwd="/opt/ProviderApp", wait_on_exit=True, **kwargs)


def call_consumer_and_provider(target,
                               consumer_criticality: Criticality,
                               consumer_expects_criticality: Criticality,
                               provider_criticality: Criticality,
                               timeout_sec=60):
    """
    Validates:
    - Method registration on a skeleton with a given provider_criticality
    - Method invocation from a proxy with a given consumer_criticality
    - Correct parameter passing and result return
    - Both copy and zero-copy call semantics are tested
    Note: every even numbered method call is zero-copy, thus more
    than two methods need to be registered, in the used mw_com_config.json
    """
    print(f"Starting consumer and provider with criticality {
          consumer_criticality.value} and {provider_criticality.value}")

    with provider(target, provider_criticality):
        with consumer(target, consumer_criticality, consumer_expects_criticality):
            pass

