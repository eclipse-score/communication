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
"""Bidirectional integration test for the mw::com hypervisor gateway.

Runs 4 real processes across 2 QNX QEMU VMs, communicating through the
ivshmem-backed gateway transport:
  - VM-B (target_b): provider_app (skeleton) + gateway in "sender" mode
  - VM-A (target_a): consumer_app (proxy) + gateway in "receiver" mode

The provider_app publishes samples; the sender gateway forwards them over
ivshmem to the receiver gateway on VM-A, which republishes them locally for
consumer_app to pick up. consumer_app touches a sentinel file once it has
received enough samples, which this test polls for as proof that the full
publish -> sender-gateway -> receiver-gateway -> subscribe chain works.
"""

import logging
import time

logger = logging.getLogger(__name__)

SUCCESS_SENTINEL = "/tmp/hv_gateway_test_success"
SENTINEL_TIMEOUT_SECONDS = 60
SENTINEL_POLL_INTERVAL_SECONDS = 2


def consumer_app(target, config, **kwargs):
    args = ["--service-instance-manifest", f"./etc/{config}"]
    return target.wrap_exec("bin/consumer_app", args, cwd="/opt/consumer_app", **kwargs)


def provider_app(target, config, **kwargs):
    args = ["--service-instance-manifest", f"./etc/{config}"]
    return target.wrap_exec("bin/provider_app", args, cwd="/opt/provider_app", **kwargs)


def consumer_gateway(target, config, gateway_config, **kwargs):
    args = [
        "--service-instance-manifest",
        f"./etc/{config}",
        "--gateway-config",
        f"./etc/{gateway_config}",
        "--mode",
        "receiver",
    ]
    return target.wrap_exec("bin/gateway_app", args, cwd="/opt/gateway_app", **kwargs)


def provider_gateway(target, config, gateway_config, **kwargs):
    args = [
        "--service-instance-manifest",
        f"./etc/{config}",
        "--gateway-config",
        f"./etc/{gateway_config}",
        "--mode",
        "sender",
    ]
    return target.wrap_exec("bin/gateway_app", args, cwd="/opt/gateway_app", **kwargs)


def _wait_for_sentinel(target, path, timeout_seconds, interval_seconds):
    """Poll (via SSH) until *path* exists on *target*, or raise on timeout."""
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        rc, _ = target.execute(f"test -e {path}")
        if rc == 0:
            return
        time.sleep(interval_seconds)
    raise TimeoutError(f"Success sentinel {path!r} did not appear within {timeout_seconds}s")


def test_hv_gateway(target_a, target_b):
    """Bidirectional smoke test: provider (VM-B) -> sender gateway -> receiver gateway -> consumer (VM-A)."""
    with provider_app(target_b, "mw_com_config.json"):
        with provider_gateway(target_b, "mw_com_config_sender.json", "mw_com_gateway_config_sender.json"):
            with consumer_app(target_a, "mw_com_config.json"):
                with consumer_gateway(target_a, "mw_com_config_receiver.json", "mw_com_gateway_config_receiver.json"):
                    _wait_for_sentinel(
                        target_a,
                        SUCCESS_SENTINEL,
                        SENTINEL_TIMEOUT_SECONDS,
                        SENTINEL_POLL_INTERVAL_SECONDS,
                    )
