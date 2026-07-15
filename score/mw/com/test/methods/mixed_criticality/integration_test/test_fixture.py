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

from contextlib import contextmanager
from enum import Enum


def service_discovery_daemon(target):
    @contextmanager
    def _service_discovery_daemon():
        daemon_process = target.execute_async(
            "bin/service_discovery_daemon_app",
            args=[],
            cwd="/opt/ServiceDiscoveryDaemonApp",
        )
        yield daemon_process

    return _service_discovery_daemon()


class Criticality(Enum):
    ASILB = "asil_b"
    QM = "qm"


def consumer(target, consumer_criticality: Criticality, consumer_expects_criticality: Criticality = None, **kwargs):
    if consumer_expects_criticality is None:
        consumer_expects_criticality = consumer_criticality

    if consumer_criticality == consumer_expects_criticality:
        consumer_config_name = (f"{consumer_criticality.value}_to_{consumer_criticality.value}_mw_com_config.json")
    else:
        # consumer app may be asil b but consume asil qm provider thus two separate
        # Criticality values need to be specified here
        consumer_config_name = (
            f"consumer_{consumer_criticality.value}_to_{consumer_expects_criticality.value}_mw_com_config.json")

    args = ["--service_instance_manifest", f"./etc/{consumer_config_name}"]
    return target.wrap_exec("bin/consumer_main", args, cwd="/opt/ConsumerApp", wait_on_exit=True, **kwargs)


def provider(target, provider_criticality: Criticality, **kwargs):
    # For a provider the global app criticality and the
    # offered criticality is always the same
    provider_config_name = (
        f"{provider_criticality.value}_to_{provider_criticality.value}_mw_com_config.json")

    args = ["--service_instance_manifest", f"./etc/{provider_config_name}"]
    return target.wrap_exec("bin/provider_main", args, cwd="/opt/ProviderApp", wait_on_exit=True, **kwargs)

