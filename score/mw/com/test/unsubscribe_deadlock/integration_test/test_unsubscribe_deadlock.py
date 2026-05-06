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

"""Integration test for unsubscribe_deadlock."""


def unsubscribe_deadlock(target, **kwargs):
    args = []
    return target.wrap_exec(
        "bin/unsubscribe_deadlock", args, cwd="/opt/unsubscribe_deadlock", wait_on_exit=True, **kwargs
    )


def test_unsubscribe_deadlock(target):
    """Test that Unsubscribe doesn't deadlock when called concurrently with GetSubscriptionState.

    This is a loopback test where both skeleton and proxy run in the same process.
    It tests that calling Unsubscribe() while GetSubscriptionState() is being called
    from a receive handler doesn't cause a deadlock.
    """
    with unsubscribe_deadlock(target, wait_timeout=15):
        pass
