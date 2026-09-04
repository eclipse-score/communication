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
from contextlib import ExitStack


# Number of consumer processes. Each consumer will spawn multiple identical proxies which
# enable a different combination of methods. There must be at least as many configuration files as NUM_CONSUMERS 
# which are named consumer0_mw_com_config.json, consumer1_mw_com_config.json, etc. kEnabledMethodsPerProxy 
# must be at least as large as NUM_CONSUMERS.
NUM_CONSUMERS = 4


# Number of proxies that are created per consumer process. Each proxy will be spawned in a separate 
# thread. This can be freely configured.
NUM_PROXIES_PER_CONSUMER = 5


# Number of calls to each enabled method per proxy. This can be freely configured.
NUM_METHOD_CALLS_PER_PROXY = 2


def provider(target, num_consumers, num_proxies_per_consumer, num_method_calls_per_proxy, config_name, **kwargs):
    args = ["--num-consumers", str(num_consumers),
            "--num-proxies-per-consumer", str(num_proxies_per_consumer), 
            "--num-method-calls-per-proxy", str(num_method_calls_per_proxy),
            "--service-instance-manifest", f"./etc/{config_name}"]
    return target.wrap_exec("bin/main_provider", args, cwd="/opt/MainProviderApp", wait_on_exit=True, wait_timeout=120, **kwargs)


def consumer(target, consumer_id, num_proxies_per_consumer, num_method_calls_per_proxy, config_name, **kwargs):
    args = ["--consumer-id", str(consumer_id),
            "--num-proxies-per-consumer", str(num_proxies_per_consumer), 
            "--num-method-calls-per-proxy", str(num_method_calls_per_proxy),
            "--service-instance-manifest", f"./etc/{config_name}"]
    return target.wrap_exec("bin/main_consumer", args, cwd="/opt/MainConsumerApp", wait_on_exit=True, wait_timeout=120, **kwargs)


def test_multiple_proxies_different_processes(target):
    with provider(target, NUM_CONSUMERS, NUM_PROXIES_PER_CONSUMER, NUM_METHOD_CALLS_PER_PROXY, "provider_mw_com_config.json"):
        # Launch NUM_CONSUMERS consumers using the python config manager (i.e. `with` keyword). We use ExitStack to ensure that the __exit__ function of each launched consumer (returned from consumer()) is called.
        with ExitStack() as stack:
            for i in range(NUM_CONSUMERS):
                stack.enter_context(consumer(target, i, NUM_PROXIES_PER_CONSUMER, NUM_METHOD_CALLS_PER_PROXY, f"consumer{i}_mw_com_config.json"))

