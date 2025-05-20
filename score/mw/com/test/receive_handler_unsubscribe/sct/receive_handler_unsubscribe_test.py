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
    def __init__(self, environment, cycle_time=None, num_cycles=None, **kwargs):
        args = [
        ]
        if num_cycles is not None:
            args += ["-n", str(num_cycles)]
        if cycle_time is not None:
            args += ["-t", str(cycle_time)]
        wait_on_exit = num_cycles is not None
        super().__init__(environment, "bin/receive_handler_unsubscribe", args,
                         cwd="/opt/receive_handler_unsubscribe", use_sandbox=True,
                         wait_on_exit=wait_on_exit, **kwargs)


# See documentation in ITF version of test (platform/aas/test/mw/com/test_receive_handler_unsubscribe.py)
def test_lola_receive_handler_unsubscribe(environment):
    with Bigdata(environment, cycle_time=40, num_cycles=25, wait_timeout=15):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
