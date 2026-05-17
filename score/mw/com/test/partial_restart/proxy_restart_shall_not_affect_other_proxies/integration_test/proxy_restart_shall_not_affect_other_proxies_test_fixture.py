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


def proxy_restart_shall_not_affect_other_proxies(target, number_of_consumer_restarts, kill_consumer, **kwargs):
    args = ["--kill", f"{kill_consumer}", "--number-consumer-restarts", f"{number_of_consumer_restarts}"]
    return target.wrap_exec(
        "./bin/proxy_restart_shall_not_affect_other_proxies",
        args,
        cwd="/opt/proxy_restart_shall_not_affect_other_proxies",
        wait_on_exit=True,
        **kwargs,
    )
