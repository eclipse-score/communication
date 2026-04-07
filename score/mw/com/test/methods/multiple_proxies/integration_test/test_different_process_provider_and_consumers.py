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
NUM_PROXIES_PER_CONSUMER = [7, 3, 3, 10]
NUM_CONSUMERS = len(NUM_PROXIES_PER_CONSUMER)


def start_consumer_and_wait(sut, i: int):
    if i > NUM_CONSUMERS - 1:
        return
    print(f"Starting consumer {i}...")
    config_name = f"consumer{i}_mw_com_config.json"
    num_proxies = NUM_PROXIES_PER_CONSUMER[i]
    args = f"--consumer_id {i} --num-proxies-per-consumer {num_proxies} "
    args += f"--service-instance-manifest ./etc/{config_name}"

    with sut.start_process(
        f"./bin/consumer_main {args}",
            cwd=f"/opt/Consumer{i}App/") as consumer_process:
        start_consumer_and_wait(sut, i+1)
        print(f"Waiting for consumer {i} to exit...")
        assert consumer_process.wait_for_exit(timeout=TIMEOUT_SEC) == 0


def test_multiple_proxies(sut):

    args = ""

    for i in range(NUM_CONSUMERS):
        args += f"--num-proxies-per-consumer{i} {NUM_PROXIES_PER_CONSUMER[i]} "
    print(f"Starting provider with args: {args}")

    with sut.start_process(f"./bin/provider_main {args}",
                           cwd="/opt/ProviderApp/") as provider_process:
        start_consumer_and_wait(sut, 0)
        assert provider_process.wait_for_exit(timeout=TIMEOUT_SEC) == 0
