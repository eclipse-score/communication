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

"""Integration test to verify Skeleton crash recovery and system integrity."""

import time
import pytest

def test_skeleton_restart_integrity(sut):
    """Verifies that a new skeleton can be started and discovered after a previous one was killed."""
    
    app_bin = "./bin/feat_com_communication_app"
    app_cwd = "/opt/feat_com_communication/"

    # 1. Start the first Skeleton
    print("Starting Skeleton A...")
    with sut.start_process(f"{app_bin} --mode send -t 50 -n 200", cwd=app_cwd) as skeleton_a:
        time.sleep(3.0)

        print("Starting Proxy A...")
        with sut.start_process(f"{app_bin} --mode recv -n 5", cwd=app_cwd) as proxy_a:
            assert proxy_a.wait_for_exit(timeout=30) == 0
            print("Proxy A finished successfully.")

        print("Force-killing Skeleton A (SIGKILL)...")
        sut.execute("pkill -9 -f mode\\ send")
        
        for _ in range(5):
            exit_code, output = sut.execute("pgrep -f mode\\ send")
            if exit_code != 0:
                print("Confirmed: Skeleton A process is gone.")
                break
            time.sleep(1.0)
        else:
            print(f"Warning: Skeleton A might still be running! pgrep output: {output}")

    print("Waiting for system to settle and cleaning artifacts...")
    time.sleep(5.0)
    sut.execute("rm -rf /tmp/mw_com_lola")

    print("Starting Skeleton B (Recovery instance)...")
    with sut.start_process(f"{app_bin} --mode send -t 50 -n 100", cwd=app_cwd) as skeleton_b:
        time.sleep(3.0)
        print("Starting Proxy B...")
        with sut.start_process(f"{app_bin} --mode recv -n 5", cwd=app_cwd) as proxy_b:
            assert proxy_b.wait_for_exit(timeout=30) == 0
            print("Proxy B finished successfully. System recovered.")

        assert skeleton_b.wait_for_exit(timeout=60) == 0
