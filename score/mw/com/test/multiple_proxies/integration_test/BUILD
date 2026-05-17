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
    name = "multiple_proxies",
    timeout = "moderate",
    srcs = ["test_multiple_proxies.py"],
    filesystem = "//score/mw/com/test/multiple_proxies:multiple_proxies-pkg",
)
