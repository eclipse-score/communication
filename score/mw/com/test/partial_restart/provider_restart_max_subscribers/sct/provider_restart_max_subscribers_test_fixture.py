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

from sctf.sim.base_sim import BaseSim


class ProviderRestartMaxSubscribers(BaseSim):
    def __init__(self, environment, is_proxy_connected_during_restart, **kwargs):
        args = ["--is-proxy-connected-during-restart", f"{is_proxy_connected_during_restart}"]
        super().__init__(
            environment,
            "bin/provider_restart_max_subscribers_application",
            args,
            cwd="/opt/provider_restart_max_subscribers",
            use_sandbox=True,
            wait_on_exit=True,
            **kwargs
        )
