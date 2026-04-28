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


def provider_restart_max_subscribers(target, is_proxy_connected_during_restart, **kwargs):
    args = [
        "--is-proxy-connected-during-restart",
        f"{is_proxy_connected_during_restart}",
        "--iterations",
        "3",
        "--service_instance_manifest",
        "etc/mw_com_config.json",
    ]
    return target.wrap_exec(
        "bin/provider_restart_max_subscribers_application",
        args,
        cwd="/opt/provider_restart_max_subscribers",
        wait_on_exit=True,
        wait_timeout=120,
        **kwargs,
    )
