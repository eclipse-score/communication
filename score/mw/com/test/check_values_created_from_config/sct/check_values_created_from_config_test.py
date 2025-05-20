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

class CheckValuesCreatedFromConfig(BaseSim):
    def __init__(self, environment, mode, **kwargs):
        args = [
            "-service_instance_manifest", "./etc/mw_com_config.json",
            "--mode", mode
        ]
        wait_on_exit = True
        super().__init__(environment, "bin/check_values_created_from_config", args,
                         cwd="/opt/check_values_created_from_config", use_sandbox=True,
                         wait_on_exit=wait_on_exit, **kwargs)


# See documentation in ITF version of test (platform/aas/test/mw/com/test_mw_com_config.py)
def test_check_values_created_from_config(environment):
    with CheckValuesCreatedFromConfig(environment, mode="send", wait_timeout=15), \
            CheckValuesCreatedFromConfig(environment, mode="recv", wait_timeout=15):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
