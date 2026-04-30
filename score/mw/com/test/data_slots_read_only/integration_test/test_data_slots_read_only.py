# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
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

"""Integration test for data_slots_read_only."""


def data_slots_read_only(target, mode, should_modify_data_segment, cycle_time=None, num_cycles=None, **kwargs):
    args = ["--mode", mode, "--should-modify-data-segment", "true" if should_modify_data_segment else "false"]
    if num_cycles is not None:
        args += ["--num-cycles", str(num_cycles)]
    if cycle_time is not None:
        args += ["--cycle-time", str(cycle_time)]
    wait_on_exit = num_cycles is not None
    return target.wrap_exec(
        "bin/data_slots_read_only", args, cwd="/opt/data_slots_read_only", wait_on_exit=wait_on_exit, **kwargs
    )


def test_data_slots_read_only(target):
    """Test data slots read-only functionality."""
    sigabort_return_code = 134
    sigsegv_return_code = 139
    sanitizer_error_code = 55
    expected_return_codes = [sigabort_return_code, sigsegv_return_code, sanitizer_error_code]

    # Running the test without modification of the data segment should pass
    with data_slots_read_only(target, "send", should_modify_data_segment=False, cycle_time=10, num_cycles=100, wait_timeout=30):
        with data_slots_read_only(
            target, "recv", should_modify_data_segment=False, num_cycles=25, wait_timeout=30
        ) as receiver:
            pass

    # Running the test with modification of the data segment should not pass.
    # The receiver is expected to crash (SIGABRT, SIGSEGV, or sanitizer error).
    # The sender may also fail (e.g. SIGPIPE) when the receiver's IPC endpoint dies.
    actual_return_code = None
    try:
        with data_slots_read_only(target, "send", should_modify_data_segment=True, cycle_time=10, num_cycles=100, wait_timeout=60):
            try:
                with data_slots_read_only(
                    target, "recv", should_modify_data_segment=True, num_cycles=25, wait_timeout=60
                ) as receiver:
                    pass
            except RuntimeError:
                actual_return_code = receiver.ret_code
    except RuntimeError:
        pass  # Sender may fail with SIGPIPE when receiver crashes

    assert actual_return_code is not None, "Expected RuntimeError was not raised"
    assert actual_return_code in expected_return_codes, (
        f"Application exit code: {receiver.ret_code} is not equal to one of the expected return code: {expected_return_codes}"
    )
