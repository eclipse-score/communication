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


class Client(BaseSim):
    def __init__(self, environment, **kwargs):
        args = ["-r", "20", "-b", "50"]
        super().__init__(environment, "bin/client", args, cwd="/opt/ClientApp", use_sandbox=True, wait_on_exit=True,
                         **kwargs)


class Service(BaseSim):
    def __init__(self, environment, **kwargs):
        args = ["-t", "250"]
        super().__init__(environment, "bin/service", args, cwd="/opt/ServiceApp", use_sandbox=True, **kwargs)

def test_field_initial_value(adaptive_environment_fixture):
    with Service(adaptive_environment_fixture):
        with Client(adaptive_environment_fixture):
            pass


if __name__ == "__main__":
    sctf.run(__file__)
