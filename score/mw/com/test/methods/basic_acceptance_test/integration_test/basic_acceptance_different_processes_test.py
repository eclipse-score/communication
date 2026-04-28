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


def provider(target, **kwargs):
    args = []
    return target.wrap_exec("bin/main_provider", args, cwd="/opt/MainProviderApp", wait_on_exit=True, **kwargs)


def consumer(target, **kwargs):
    args = []
    return target.wrap_exec("bin/main_consumer", args, cwd="/opt/MainConsumerApp", wait_on_exit=True, **kwargs)


def test_basic_acceptance_different_processes_test(target):
    """Test method call functionality between provider and consumer in different processes.

    The first process creates a Skeleton instance with a method (InArgs and Return value),
    verifying the service can be offered and the handler successfully registered.

    The second process creates a Proxy instance subscribing to the Skeleton, calling
    zero-copy and with-copy methods, verifying both succeed with expected return values.
    """
    with provider(target), consumer(target):
        pass
