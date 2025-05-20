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


class Bigdata(BaseSim):
    def __init__(self, environment, **kwargs):
        args = []
        wait_on_exit = True
        super().__init__(environment, "bin/concurrent_skeleton_creation", args,
                         cwd="/opt/concurrent_skeleton_creation", use_sandbox=True,
                         wait_on_exit=wait_on_exit, **kwargs)


def test_concurrent_skeleton_creation(environment):
    with Bigdata(environment, wait_timeout=15):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
