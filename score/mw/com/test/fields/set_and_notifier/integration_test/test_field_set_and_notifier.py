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


def client(target, mode, **kwargs):
    args = ["--num-retries", "20", "--backoff-time", "50", "--mode", mode]
    return target.wrap_exec("bin/main_client", args, cwd="/opt/MainClientApp", wait_on_exit=True, **kwargs)


def service(target, mode, **kwargs):
    args = ["--cycle-time", "250", "--mode", mode]
    return target.wrap_exec("bin/main_service", args, cwd="/opt/MainServiceApp", **kwargs)


def test_field_notifier_initial_value(target):
    """Test field initial value exchange between service and client."""
    with service(target, "notifier"):
        with client(target, "notifier"):
            pass


def test_field_set_value(target):
    """Test field set exchange and accepted value propagation between service and client."""
    with service(target, "set"):
        with client(target, "set"):
            pass


# TODO: Add a dedicated get scenario test once getter-enabled field mode is available.
