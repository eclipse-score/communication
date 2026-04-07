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


TIMEOUT_SEC = 120
NUM_PROXIES_PER_CONSUMER = [7, 3, 3, 1]
NUM_CONSUMERS = len(NUM_PROXIES_PER_CONSUMER)


def test_multiple_proxies(sut):
    args = ""
    for i in range(NUM_CONSUMERS):
        num_proxies = NUM_PROXIES_PER_CONSUMER[i]
        args += f"--num-proxies-per-consumer{i} {num_proxies} "

    config_name = "mw_com_config.json"
    args += f"--service-instance-manifest ./etc/{config_name}"

    with sut.start_process(
            f"./bin/combined_consumer_provider_main {args}",
            cwd="/opt/CombinedConsumerProviderApp/") as combinded_process:
        assert combinded_process.wait_for_exit(timeout=TIMEOUT_SEC) == 0
