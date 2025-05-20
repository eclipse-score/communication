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


class GenericProxy(BaseSim):
    def __init__(self, environment, mode, cycle_time=None, num_cycles=None, **kwargs):
        args = [
            "--mode", mode
        ]
        if num_cycles is not None:
            args += ["-n", str(num_cycles)]
        if cycle_time is not None:
            args += ["-t", str(cycle_time)]
        wait_on_exit = num_cycles is not None
        super().__init__(environment, "bin/generic_proxy", args,
                         cwd="/opt/generic_proxy", use_sandbox=True,
                         wait_on_exit=wait_on_exit, **kwargs)


# See documentation in ITF version of test (platform/aas/test/mw/com/test_mw_com_generic_proxy.py)
def test_lola_generic_proxy(environment):
    with GenericProxy(environment, "send", cycle_time=40), GenericProxy(environment, "recv",
                                                                        num_cycles=25,
                                                                        wait_timeout=15):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
