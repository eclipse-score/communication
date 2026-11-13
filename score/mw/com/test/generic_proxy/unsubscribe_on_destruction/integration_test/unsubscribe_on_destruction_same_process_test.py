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
    args = ["--service_instance_manifest", "./etc/mw_com_config.json"]
    return target.wrap_exec(
        "bin/main_consumer_and_provider",
        args,
        cwd="/opt/MainConsumerAndProviderApp",
        wait_on_exit=True,
        **kwargs,
    )


def test_unsubscribe_on_destruction_same_process_test(target):
    """Test that a GenericProxy unsubscribes its events on destruction, provider and consumer in the same process.

    The test starts a single process with two threads. The first thread creates a Skeleton
    instance with an event, offers it and sends samples once the consumer has subscribed.
    The second thread creates a GenericProxy, subscribes to the event, receives the samples
    and then destroys the GenericProxy without calling Unsubscribe(). The process must exit
    successfully, i.e. the GenericProxy has to clean up its subscription on destruction.
    """
    with consumer_and_provider(target):
        pass
