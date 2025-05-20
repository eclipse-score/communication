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


class DataSlotsReadOnly(BaseSim):
    def __init__(self, environment, mode, should_modify_data_segment, cycle_time=None, num_cycles=None, **kwargs):
        args = [
            "--mode", mode,
            "--should-modify-data-segment", "true" if should_modify_data_segment else "false"
        ]
        if num_cycles is not None:
            args += ["-n", str(num_cycles)]
        if cycle_time is not None:
            args += ["-t", str(cycle_time)]
        wait_on_exit = num_cycles is not None
        super().__init__(environment, "bin/data_slots_read_only", args,
                         cwd="/opt/data_slots_read_only", use_sandbox=True,
                         wait_on_exit=wait_on_exit, **kwargs)


# See documentation in ITF version of test (platform/aas/test/mw/com/test_data_slots_read_only.py)
def test_lola_data_slots_read_only(environment):
    sigabort_return_code = 134
    sigsegv_return_code = 139
    expected_return_codes = [sigabort_return_code, sigsegv_return_code]

    # Running the test without modification of the data segment should pass
    with DataSlotsReadOnly(environment, "send", should_modify_data_segment=False, cycle_time=40):
        with DataSlotsReadOnly(environment, "recv", should_modify_data_segment=False,
                               num_cycles=25,
                               wait_timeout=15) as receiver:
            pass

    # Running the test with modification of the data segment should not pass
    with DataSlotsReadOnly(environment, "send", should_modify_data_segment=True, cycle_time=40):
        try:
            with DataSlotsReadOnly(environment, "recv", should_modify_data_segment=True,
                                   num_cycles=25,
                                   wait_timeout=15) as receiver:
                pass
        except sctf.SctfAssertionError:
            pass

    actual_return_code = receiver.ret_code
    assert actual_return_code in expected_return_codes, \
        f"Application exit code: {receiver.ret_code} is not equal to one of the expected return code: {expected_return_codes}"


if __name__ == "__main__":
    sctf.run(__file__)
