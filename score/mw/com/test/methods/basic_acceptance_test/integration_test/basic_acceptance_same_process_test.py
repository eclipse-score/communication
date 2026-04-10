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

def test_basic_acceptance_same_process_test(sut):
    """"
    Test method call functionality between provider and consumer which are run in the same process.

    The test starts a single process which spawns two threads.
    The first thread creates a Skeleton instance which contains a method with InArgs and Return value.
    This thread checks that the service can be offered and the handler successfully registered.

    The second thread creates a Proxy instance which subscribes to the Skeleton in the first thread.
    This thread calls a zero-copy and with-copy method call and verifies that both calls succeed and return the expected value.

    Test is successful if all previous checks return true and we have no crashes.
    """
    with sut.start_process("./bin/main_consumer_and_provider", cwd="/opt/MainConsumerAndProviderApp/") as consumer_and_sender:
        assert consumer_and_sender.wait_for_exit(timeout=120) == 0
