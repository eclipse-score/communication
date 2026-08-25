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
load("//quality/integration_testing:integration_testing.bzl", "integration_test")

integration_test(
    name = "test_add_on_config_loading",
    timeout = "moderate",
    srcs = [
        "test_loading_add_on_config.py",
    ],
    filesystem = "//score/mw/com/test/loading_add_on_configuration:add_on_loading-pkg",
)
