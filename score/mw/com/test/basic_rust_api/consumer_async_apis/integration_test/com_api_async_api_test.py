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


def producer(target, num_cycles, **kwargs):
    args = ["-n", str(num_cycles)]
    return target.wrap_exec("bin/bigdata-producer", args, cwd="/opt/bigdata-com-api-async", **kwargs)


def consumer_async(target, num_cycles, **kwargs):
    args = ["-n", str(num_cycles)]
    return target.wrap_exec(
        "bin/bigdata-consumer-async", args, cwd="/opt/bigdata-com-api-async", wait_on_exit=True, **kwargs
    )


def test_bigdata_async_exchange(target):
    # Sender runs for 40 cycles, Receiver receives 25 cycles asynchronously
    with producer(target, num_cycles=40), consumer_async(target, num_cycles=25, wait_timeout=120):
        pass
