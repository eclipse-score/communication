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
from sctf.sim.base_sim import BaseSim


# See documentation in ITF version of test (platform/aas/test/mw/com/test_partial_restart_proxy_restart_shall_not_affect_other_proxies.py)
class ProxyRestartShallNotAffectOtherProxies(BaseSim):
    def __init__(self, environment, number_of_consumer_restarts, kill_consumer, **kwargs):
        args = ["--kill", f"{kill_consumer}", "-- number-consumer-restarts", f"{number_of_consumer_restarts}"]
        super().__init__(
            environment,
            "bin/proxy_restart_shall_not_affect_other_proxies",
            args,
            cwd="/opt/proxy_restart_shall_not_affect_other_proxies",
            use_sandbox=True,
            wait_on_exit=True,
            **kwargs
        )

