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


import signal

# 128 + SIGABRT: expected exit code when std::terminate() is invoked, e.g. because merging an invalid add-on
# configuration is rejected by the mw::com runtime.
SIGABRT_EXIT_CODE = 128 + signal.SIGABRT


def test_add_on_loading(target):
    """Test loading an add-on application with communication between provider and consumer. Add-on configuration is
    merged in between communication cycles."""
    with provider(target, "mw_com_config.json"):
        with consumer(target, "mw_com_config.json"):
            pass


def test_add_on_merge_during_active_communication(target):
    """Test that merging the add-on configuration while the base service is already sending does not disrupt it.

    The provider merges the (valid) add-on configuration synchronously in the middle of its 1st publish loop.
    The base service consumer is expected to receive its samples normally and the add-on service is expected to work
    normally after the merge.
    """
    with provider(target, "mw_com_config.json", merge_during_stream=True):
        with consumer(target, "mw_com_config.json"):
            pass


def test_invalid_add_on_loading(target):
    """Test that loading an invalid add-on configuration (duplicate service identifier) is rejected.

    Merging the invalid add-on configuration is expected to make the mw::com runtime call std::terminate(), so the
    provider/consumer processes are expected to be killed with SIGABRT rather than exit normally.
    """
    with provider(target, "mw_com_config.json", invalid_addon_only=True, expected_exit_code=SIGABRT_EXIT_CODE):
        pass
    with consumer(target, "mw_com_config.json", invalid_addon_only=True, expected_exit_code=SIGABRT_EXIT_CODE):
        pass


def test_asymmetric_add_on_merge(target):
    """Test that a consumer which never merged the add-on configuration cannot discover the add-on service.

    The provider merges the add-on configuration and offers the add-on service, while on the consumer side it is never
    merged and the consumer directly attempts to find/create a proxy for the add-on service instance. Since the add-on
    instance specifier is unknown to the consumer's local configuration, service discovery is expected to fail and
    the consumer is expected to exit gracefully and no crash is expected.
    """
    with provider(target, "mw_com_config.json", offer_addon_only=True, wait_on_exit=True):
        with consumer(target, "mw_com_config.json", addon_no_merge=True, expected_exit_code=1):
            pass


def consumer(target, config, invalid_addon_only=False, addon_no_merge=False, **kwargs):
    args = [
        "--service-instance-manifest",
        f"./etc/{config}",
        "--addon_manifest",
        f"./etc/mw_com_add_on_config.json",
        "--invalid_addon_manifest",
        f"./etc/mw_com_invalid_add_on_config.json",
    ]
    if invalid_addon_only:
        args += ["--invalid-addon-only", "true"]
    if addon_no_merge:
        args += ["--addon-no-merge", "true"]
    return target.wrap_exec("bin/consumer", args, cwd="/opt/consumer", wait_on_exit=True, **kwargs)


def provider(
    target,
    config,
    invalid_addon_only=False,
    offer_addon_only=False,
    merge_during_stream=False,
    **kwargs,
):
    args = [
        "--service-instance-manifest",
        f"./etc/{config}",
        "--addon_manifest",
        f"./etc/mw_com_add_on_config.json",
        "--invalid_addon_manifest",
        f"./etc/mw_com_invalid_add_on_config.json",
    ]
    if invalid_addon_only:
        args += ["--invalid-addon-only", "true"]
    if offer_addon_only:
        args += ["--offer-addon-only", "true"]
    if merge_during_stream:
        args += ["--merge-during-stream", "true"]
    kwargs.setdefault("wait_on_exit", invalid_addon_only or offer_addon_only)
    return target.wrap_exec("bin/provider", args, cwd="/opt/provider", **kwargs)
