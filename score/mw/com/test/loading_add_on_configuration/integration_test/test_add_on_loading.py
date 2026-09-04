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


def test_add_on_loading(target):
    """Test loading an add-on application with communication between provider and consumer"""
    with provider(target, "mw_com_config.json"):
        with consumer(target, "mw_com_config.json"):
            pass


def consumer(target, config, **kwargs):
    args = ["--service-instance-manifest", f"./etc/{config}", "--addon_manifest", f"./etc/mw_com_add_on_config.json"]
    return target.wrap_exec("bin/consumer", args, cwd="/opt/consumer", wait_on_exit=True, **kwargs)


def provider(target, config, **kwargs):
    args = ["--service-instance-manifest", f"./etc/{config}", "--addon_manifest", f"./etc/mw_com_add_on_config.json"]
    return target.wrap_exec("bin/provider", args, cwd="/opt/provider", **kwargs)
