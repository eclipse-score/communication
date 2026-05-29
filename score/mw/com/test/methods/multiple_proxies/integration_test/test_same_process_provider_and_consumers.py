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


# Number of consumers. Each consumer will spawn multiple identical proxies which enable 
# a different combination of methods. These consumers will be in the same process. There 
# must be at least as many configuration files as NUM_CONSUMERS which are named 
# consumer0_mw_com_config.json, consumer1_mw_com_config.json, etc. kEnabledMethodsPerProxy 
# must be at least as large as NUM_CONSUMERS.
NUM_CONSUMERS = 4


# Number of proxies that are created in the consumer process. Each proxy will be spawned in a separate 
# thread. This can be freely configured.
NUM_PROXIES_PER_CONSUMER = 5


# Number of calls to each enabled method per proxy. This can be freely configured.
NUM_CALLS_PER_METHOD = 10


def provider_and_consumer(target, num_consumers, num_proxies_per_consumer, NUM_CALLS_PER_METHOD, config_name, **kwargs):
    args = ["--num-consumers", str(num_consumers),
            "--num-proxies-per-consumer", str(num_proxies_per_consumer), 
            "--num-method-calls-per-proxy", str(NUM_CALLS_PER_METHOD),
            "--service-instance-manifest", f"./etc/{config_name}"]
    return target.wrap_exec("bin/main_combined_consumer_provider", args, cwd="/opt/MainCombinedConsumerProviderApp", wait_on_exit=True, **kwargs)

def test_multiple_proxies_same_process(target):
    with provider_and_consumer(target, NUM_CONSUMERS, NUM_PROXIES_PER_CONSUMER, NUM_CALLS_PER_METHOD, "combined_mw_com_config.json") as provider_and_consumer_process:
        pass
