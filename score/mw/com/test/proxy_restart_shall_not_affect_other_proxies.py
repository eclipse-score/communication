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

import sctf
from sctf import BaseSim


class proxy_restart_shall_not_affect_other_proxies(BaseSim):
    def __init__(self, environment, **kwargs):
        args = [""]
        super().__init__(
            environment,
            "bin/proxy_restart_shall_not_affect_other_proxies",
            args,
            cwd="/opt/proxy_restart_shall_not_affect_other_proxies",
            use_sandbox=True,
            wait_on_exit=True,
            **kwargs
        )


def test_proxy_restart_shall_not_affect_other_proxies(basic_sandbox):
    with proxy_restart_shall_not_affect_other_proxies(basic_sandbox):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
