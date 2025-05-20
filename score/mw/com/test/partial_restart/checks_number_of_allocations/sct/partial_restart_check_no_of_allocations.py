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


# See documentation in ITF version of test (platform/aas/test/mw/com/test_partial_restart_check_number_of_allocations.py)
class CheckNumberOfAllocations(BaseSim):
    def __init__(self, environment, **kwargs):
        args = [""]
        super().__init__(
            environment,
            "bin/check_number_of_allocations",
            args,
            cwd="/opt/check_number_of_allocations",
            use_sandbox=True,
            wait_on_exit=True,
            **kwargs
        )


def test_check_no_of_allocations(adaptive_environment_fixture):
    with CheckNumberOfAllocations(adaptive_environment_fixture):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
