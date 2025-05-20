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

class ReservingSkeletonSlotsPass(BaseSim):
    def __init__(self, environment, mode, **kwargs):
        args = [
            "-service_instance_manifest", "./etc/mw_com_config.json",
            "--mode", mode,
        ]
        wait_on_exit = True
        super().__init__(environment, "bin/reserving_skeleton_slots", args,
                         cwd="/opt/reserving_skeleton_slots", use_sandbox=True,
                         wait_on_exit=wait_on_exit, **kwargs)


class ReservingSkeletonSlotsFail(BaseSim):
    def __init__(self, environment, mode, **kwargs):
        args = [
            "-service_instance_manifest", "./etc/mw_com_config.json",
            "--mode", mode,
        ]
        wait_on_exit = True
        super().__init__(environment, "bin/reserving_skeleton_slots", args,
                         cwd="/opt/reserving_skeleton_slots", use_sandbox=True,
                         wait_on_exit=wait_on_exit, **kwargs)


# See documentation in ITF version of test (platform/aas/test/mw/com/test_reserving_skeleton_slots.py)
def test_lola_offering_events(environment):
    # Run each process sequentially
    with ReservingSkeletonSlotsPass(environment, "passing", wait_timeout=15):
        pass

    with ReservingSkeletonSlotsFail(environment, "failing_extra_slots", wait_timeout=15):
        pass

    with ReservingSkeletonSlotsFail(environment, "failing_less_slots", wait_timeout=15):
        pass


if __name__ == "__main__":
    sctf.run(__file__)
