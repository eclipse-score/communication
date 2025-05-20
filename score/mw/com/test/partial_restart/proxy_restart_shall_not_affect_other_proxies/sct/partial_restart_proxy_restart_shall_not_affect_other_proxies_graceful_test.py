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
from partial_restart_proxy_restart_shall_not_affect_other_proxies_fixture import ProxyRestartShallNotAffectOtherProxies


# Test configuration
NUMBER_OF_CONSUMER_RESTARTS = 20
KILL_CONSUMER = 0

def test_proxy_restart_shall_not_affect_other_proxies_graceful(adaptive_environment_fixture):
    with ProxyRestartShallNotAffectOtherProxies(adaptive_environment_fixture, NUMBER_OF_CONSUMER_RESTARTS, KILL_CONSUMER):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
