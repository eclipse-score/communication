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

import os

import sctf
from sctf.sim.base_sim import BaseSim

class TwofaceClient(BaseSim):
    def __init__(self, environment, **kwargs):
        os.environ["AMSR_PROCESS_SHORT_NAME_PATH"] = "/bmw/examples/ClientApp_instance"
        super().__init__(environment, "bin/client", [], cwd="/opt/ClientApp", use_sandbox=True, wait_on_exit=True,
                         **kwargs)


class TwofaceService(BaseSim):
    def __init__(self, environment, **kwargs):
        os.environ["AMSR_PROCESS_SHORT_NAME_PATH"] = "/bmw/examples/ServiceApp_instance"
        super().__init__(environment, "bin/service", [], cwd="/opt/ServiceApp", use_sandbox=True, **kwargs)


# See documentation in ITF version of test (platform/aas/test/mw/com/test_mw_com_coexistence.py)
def test_ara_com_mw_com_coexistence(adaptive_environment_fixture):
    with TwofaceService(adaptive_environment_fixture):
        with TwofaceClient(adaptive_environment_fixture):
            pass


if __name__ == "__main__":
    sctf.run(__file__)
