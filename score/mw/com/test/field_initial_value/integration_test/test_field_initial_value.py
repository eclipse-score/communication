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


def client(target, **kwargs):
    args = ["-r", "20", "-b", "50"]
    return target.wrap_exec("bin/client", args, cwd="/opt/ClientApp", wait_on_exit=True, **kwargs)


def service(target, **kwargs):
    args = ["-t", "250"]
    return target.wrap_exec("bin/service", args, cwd="/opt/ServiceApp", **kwargs)


def test_field_initial_value(target):
    """Test field initial value exchange between service and client."""
    with service(target):
        with client(target):
            pass
