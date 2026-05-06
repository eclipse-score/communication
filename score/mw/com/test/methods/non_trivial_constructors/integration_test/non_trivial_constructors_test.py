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

"""Integration test for non-trivial constructors."""


def provider(target, **kwargs):
    args = []
    return target.wrap_exec("bin/main_provider", args, cwd="/opt/MainProviderApp/", wait_on_exit=True, **kwargs)


def consumer(target, **kwargs):
    args = []
    return target.wrap_exec("bin/main_consumer", args, cwd="/opt/MainConsumerApp/", wait_on_exit=True, **kwargs)


def test_signature_variations(target):
    """
    Test method call functionality for different method signature variations between provider and consumer which are run in different processes. Each method signature contains a type which is not trivially constructible.

    The test starts two processes.
    The first process creates a Skeleton instance which contains three methods:
        - Method with InArg and Return
        - Method with InArg only
        - Method with Return only
    This process checks that the service can be offered and handlers were successfully registered for all four methods.
    This registered handlers for the method with InArg only checks that it is called with the expected arguments from the Proxy side.

    The second process creates a Proxy instance which subscribes to the Skeleton in the first process.
    This process calls a zero-copy (where possible) and with-copy method call for each of the four methods and verifies that all calls succeed and return the expected value.

    Test is successful if all previous checks return true and we have no crashes.
    """
    with provider(target, wait_timeout=120):
        with consumer(target, wait_timeout=120):
            pass
