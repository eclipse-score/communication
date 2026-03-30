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


def test_bigdata_exchange(sut):
    # Sender runs for 30 cycles, Receiver receives 25 cycles
    with sut.start_process("./bin/bigdata-producer -n 30", cwd="/opt/bigdata-com-api-sync/") as sender_process:
        with sut.start_process("./bin/bigdata-consumer -n 25", cwd="/opt/bigdata-com-api-sync/") as receiver_process:
            assert receiver_process.wait_for_exit(timeout=120) == 0

def test_mixed_primitives_exchange(sut):
    # Sender runs for 30 cycles, Receiver receives 25 cycles
    with sut.start_process("./bin/bigdata-producer -n 30 -t mixed-primitives", cwd="/opt/bigdata-com-api-sync/") as sender_process:
        with sut.start_process("./bin/bigdata-consumer -n 25 -t mixed-primitives", cwd="/opt/bigdata-com-api-sync/") as receiver_process:
            assert receiver_process.wait_for_exit(timeout=120) == 0


def test_complex_struct_exchange(sut):
    # Sender runs for 30 cycles, Receiver receives 25 cycles
    with sut.start_process("./bin/bigdata-producer -n 30 -t complex-struct", cwd="/opt/bigdata-com-api-sync/") as sender_process:
        with sut.start_process("./bin/bigdata-consumer -n 25 -t complex-struct", cwd="/opt/bigdata-com-api-sync/") as receiver_process:
            assert receiver_process.wait_for_exit(timeout=120) == 0
