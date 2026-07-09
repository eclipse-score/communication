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
def consumer_method_reenable_test(target, **kwargs):
    return target.wrap_exec(
        "bin/consumer_method_reenable_test",
        [],
        cwd="/opt/consumer_method_reenable_test",
        wait_on_exit=True,
        wait_timeout=120,
        **kwargs,
    )


def test_partial_restart_methods_consumer_method_reenable(target):
    """Test consumer restart with method re-enablement."""
    with consumer_method_reenable_test(target):
        pass
