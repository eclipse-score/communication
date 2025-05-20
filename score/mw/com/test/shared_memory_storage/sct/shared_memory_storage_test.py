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


class SharedMemoryStorage(BaseSim):
    def __init__(self, environment, mode, **kwargs):
        args = [
            "--mode", mode
        ]
        super().__init__(environment, "bin/shared_memory_storage", args,
                         cwd="/opt/shared_memory_storage", use_sandbox=True,
                         wait_on_exit=True, **kwargs)


# See documentation in ITF version of test (platform/aas/test/mw/com/test_shared_memory_storage.py)
def test_lola_shared_memory_storage(environment):
    with SharedMemoryStorage(environment, "send"), SharedMemoryStorage(environment, "recv",
                                                                       wait_timeout=15):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
