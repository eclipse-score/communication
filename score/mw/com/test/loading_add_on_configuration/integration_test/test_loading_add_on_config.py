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


def add_on_config_loading(target, mode, cycle_time=None, num_cycles=None, **kwargs):
    args = ["--mode", mode, "--addon_manifest", "./etc/mw_com_add_on_config.json"]
    if num_cycles is not None:
        args += ["--num-cycles", str(num_cycles)]
    if cycle_time is not None:
        args += ["--cycle-time", str(cycle_time)]

    return target.wrap_exec("bin/add_on_loading_app", args, cwd="/opt/add_on_loading_app", wait_on_exit=True, **kwargs)


def test_add_on_config_loading(target):
    """Tests loading and merging of add-on configurations with two applications"""
    # One application is providing services, the other one is receiving the services, both will load an
    # add-on configuration.
    # The applications perform three communication phases:
    # - providing/receiving a service defined in the initial configuration
    # - providing/receiving the same service after the add-on configuratin has been loaded and merged
    # - providing/receiving a new service that has been loaded from the add-on configuration
    # Each phase is explicitly hand-shaken via
    # a pair of InterprocessNotification objects (see RunAsSkeletonWithHandshake/
    # RunAsProxyWithHandshake in sample_sender_receiver.cpp): the skeleton only calls
    # StopOfferService() after the proxy has confirmed it received everything it needs, and the
    # proxy only starts looking for/subscribing to the service after the skeleton has confirmed it
    # is offered.
    with (
        add_on_config_loading(target, "send", cycle_time=40, num_cycles=10, wait_timeout=120),
        add_on_config_loading(target, "recv", cycle_time=40, num_cycles=10, wait_timeout=120),
    ):
        pass
