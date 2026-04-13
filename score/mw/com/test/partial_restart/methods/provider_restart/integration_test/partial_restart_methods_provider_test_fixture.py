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

def provider_restart(sut, number_restart_cycles, with_proxy, kill_provider):
    args = [
        "--kill",
        f"{kill_provider}",
        "--turns",
        f"{number_restart_cycles}",
        "--with-proxy",
        f"{with_proxy}",
        "--service_instance_manifest",
        "etc/mw_com_config.json",
    ]
    with sut.start_process(
        f"./bin/provider_restart_application {' '.join(args)}",
        cwd="/opt/provider_restart_methods",
    ) as process:
        assert process.wait_for_exit() == 0
