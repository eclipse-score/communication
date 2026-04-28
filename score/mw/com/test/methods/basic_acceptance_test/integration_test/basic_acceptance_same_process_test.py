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


def consumer_and_provider(target, **kwargs):
    args = []
    return target.wrap_exec(
        "bin/main_consumer_and_provider", args, cwd="/opt/MainConsumerAndProviderApp", wait_on_exit=True, **kwargs
    )


def test_basic_acceptance_same_process_test(target):
    """Test method call functionality between provider and consumer in the same process.

    The test starts a single process with two threads. The first thread creates a Skeleton
    instance with a method (InArgs and Return value). The second thread creates a Proxy
    subscribing to the Skeleton, calling zero-copy and with-copy methods, verifying both
    succeed with expected return values.
    """
    with consumer_and_provider(target):
        pass
