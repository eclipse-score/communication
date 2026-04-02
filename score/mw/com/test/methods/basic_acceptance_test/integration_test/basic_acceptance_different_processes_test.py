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

def test_basic_acceptance_different_processes_test(sut):
    """
    Test method call functionality between provider and consumer which are run in different processes.

    The test starts two processes.
    The first process creates a Skeleton instance which contains a method with InArgs and Return value.
    This process checks that the service can be offered and the handler successfully registered.

    The second process creates a Proxy instance which subscribes to the Skeleton in the first process.
    This process calls a zero-copy and with-copy method call and verifies that both calls succeed and return the expected value.

    Test is successful if all previous checks return true and we have no crashes.
    """
    with sut.start_process("./bin/main_provider", cwd="/opt/MainProviderApp/") as sender:
        with sut.start_process("./bin/main_consumer", cwd="/opt/MainConsumerApp/") as receiver:
            assert receiver.wait_for_exit(timeout=120) == 0
            assert sender.wait_for_exit(timeout=120) == 0
