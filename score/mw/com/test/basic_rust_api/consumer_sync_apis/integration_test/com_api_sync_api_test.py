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


def producer(target, num_cycles, data_type=None, **kwargs):
    args = ["-n", str(num_cycles)]
    if data_type is not None:
        args += ["-t", data_type]
    return target.wrap_exec("bin/bigdata-producer", args, cwd="/opt/bigdata-com-api-sync", **kwargs)


def consumer(target, num_cycles, data_type=None, **kwargs):
    args = ["-n", str(num_cycles)]
    if data_type is not None:
        args += ["-t", data_type]
    return target.wrap_exec("bin/bigdata-consumer", args, cwd="/opt/bigdata-com-api-sync", wait_on_exit=True, **kwargs)


def test_bigdata_exchange(target):
    # Sender runs for 30 cycles, Receiver receives 25 cycles
    with producer(target, num_cycles=30), consumer(target, num_cycles=25, wait_timeout=120):
        pass


def test_mixed_primitives_exchange(target):
    # Sender runs for 30 cycles, Receiver receives 25 cycles
    with (
        producer(target, num_cycles=30, data_type="mixed-primitives"),
        consumer(target, num_cycles=25, data_type="mixed-primitives", wait_timeout=120),
    ):
        pass


def test_complex_struct_exchange(target):
    # Sender runs for 30 cycles, Receiver receives 25 cycles
    with (
        producer(target, num_cycles=30, data_type="complex-struct"),
        consumer(target, num_cycles=25, data_type="complex-struct", wait_timeout=120),
    ):
        pass
