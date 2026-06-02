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

load("@rules_pkg//pkg:mappings.bzl", "pkg_filegroup")
load("//quality/integration_testing:integration_testing.bzl", "integration_test")

pkg_filegroup(
    name = "filesystem",
    srcs = [
        "//score/mw/com/test/fields/set_and_notifier:consumer-pkg",
        "//score/mw/com/test/fields/set_and_notifier:provider-pkg",
    ],
)

py_library(
    name = "test_fixture",
    srcs = ["test_fixture.py"],
)

integration_test(
    name = "test_notifier",
    timeout = "moderate",
    srcs = [
        "test_notifier.py",
        ":test_fixture",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "test_set_and_notifier",
    timeout = "moderate",
    srcs = [
        "test_set_and_notifier.py",
        ":test_fixture",
    ],
    filesystem = ":filesystem",
)
