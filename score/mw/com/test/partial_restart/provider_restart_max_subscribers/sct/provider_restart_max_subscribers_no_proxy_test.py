# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

import sctf

from provider_restart_max_subscribers_test_fixture import ProviderRestartMaxSubscribers

# Test configuration
IS_PROXY_CONNECTED_DURING_RESTART = 0


# See documentation in ITF version of test (platform/aas/test/mw/com/test_partial_restart_provider_max_subscribers_no_proxy.py)
def test_provider_restart_max_subscribers_no_proxy(environment):
    with ProviderRestartMaxSubscribers(environment, IS_PROXY_CONNECTED_DURING_RESTART):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
