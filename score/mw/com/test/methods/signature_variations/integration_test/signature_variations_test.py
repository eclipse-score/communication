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


def test_signature_variations(target):
    """Test method calls for different signature variations between provider and consumer.

    The provider Skeleton contains four methods: Method with InArg and Return,
    Method with InArg only, Method with Return only, and Method without InArg or Return.
    The consumer Proxy calls zero-copy and with-copy variants for each method,
    verifying all calls succeed and return expected values.
    """
    with provider(target), consumer(target):
        pass
