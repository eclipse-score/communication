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


class SubscribeHandler(BaseSim):
    def __init__(self, environment, mode, **kwargs):
        args = [
            "--mode", mode,
        ]
        wait_on_exit = True
        super().__init__(environment, "bin/subscribe_handler", args,
                         cwd="/opt/subscribe_handler", use_sandbox=True,
                         wait_on_exit=wait_on_exit, **kwargs)


def test_lola_subscribe_handler(environment):
    with SubscribeHandler(environment, mode="send", wait_timeout=15), \
            SubscribeHandler(environment, mode="recv", wait_timeout=15):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
