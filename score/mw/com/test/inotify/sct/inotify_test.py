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

class InotifyTest(BaseSim):
    def __init__(self, environment, **kwargs):
        args = []
        super().__init__(environment, "bin/inotify_test", args, cwd="/opt/InotifyTestApp", wait_on_exit=True, use_sandbox=True, **kwargs)

def test_find_all_semantics(adaptive_environment_fixture):
    with InotifyTest(adaptive_environment_fixture):
        pass

if __name__ == "__main__":
    sctf.run(__file__)
