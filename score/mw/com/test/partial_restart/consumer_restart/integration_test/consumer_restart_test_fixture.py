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


def partial_restart_consumer(target, number_restart_cycles, kill_consumer, **kwargs):
    args = [
        "--kill",
        f"{kill_consumer}",
        "--iterations",
        f"{number_restart_cycles}",
        "--service_instance_manifest",
        "etc/mw_com_config.json",
    ]
    return target.wrap_exec(
        "bin/consumer_restart_application",
        args,
        cwd="/opt/consumer_restart",
        wait_on_exit=True,
        wait_timeout=120,
        **kwargs,
    )
