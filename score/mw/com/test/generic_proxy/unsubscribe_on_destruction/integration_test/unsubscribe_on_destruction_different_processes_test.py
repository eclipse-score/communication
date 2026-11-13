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
    args = ["--service_instance_manifest", "./etc/mw_com_config.json"]
    return target.wrap_exec(
        "bin/main_provider",
        args,
        cwd="/opt/MainProviderApp",
        wait_on_exit=True,
        **kwargs,
    )


def consumer(target, **kwargs):
    args = ["--service_instance_manifest", "./etc/mw_com_config.json"]
    return target.wrap_exec(
        "bin/main_consumer",
        args,
        cwd="/opt/MainConsumerApp",
        wait_on_exit=True,
        **kwargs,
    )


def test_unsubscribe_on_destruction_different_processes_test(target):
    """Test that a GenericProxy unsubscribes its events on destruction, provider and consumer in different processes.

    The first process creates a Skeleton instance with an event, offers it and sends samples
    once the consumer has subscribed.

    The second process creates a GenericProxy, subscribes to the event, receives the samples
    and then destroys the GenericProxy without calling Unsubscribe(). Both processes must exit
    successfully, i.e. the GenericProxy has to clean up its subscription on destruction.
    """
    with provider(target), consumer(target):
        pass
