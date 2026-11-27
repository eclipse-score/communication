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
        "//score/mw/com/test/fields/getter_setter_if_available:consumer-pkg",
        "//score/mw/com/test/fields/getter_setter_if_available:provider-pkg",
    ],
)

pkg_filegroup(
    name = "same_process_filesystem",
    srcs = [
        "//score/mw/com/test/fields/getter_setter_if_available:consumer_and_provider-pkg",
    ],
)

integration_test(
    name = "test_notifier_only_different_process",
    timeout = "moderate",
    srcs = [
        "test_fixture.py",
        "test_notifier_only_different_process.py",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "test_notifier_only_same_process",
    timeout = "moderate",
    srcs = [
        "test_fixture.py",
        "test_notifier_only_same_process.py",
    ],
    filesystem = ":same_process_filesystem",
)

integration_test(
    name = "test_setter_only_different_process",
    timeout = "moderate",
    srcs = [
        "test_fixture.py",
        "test_setter_only_different_process.py",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "test_setter_only_same_process",
    timeout = "moderate",
    srcs = [
        "test_fixture.py",
        "test_setter_only_same_process.py",
    ],
    filesystem = ":same_process_filesystem",
)

integration_test(
    name = "test_getter_only_different_process",
    timeout = "moderate",
    srcs = [
        "test_fixture.py",
        "test_getter_only_different_process.py",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "test_getter_only_same_process",
    timeout = "moderate",
    srcs = [
        "test_fixture.py",
        "test_getter_only_same_process.py",
    ],
    filesystem = ":same_process_filesystem",
)

integration_test(
    name = "test_setter_and_getter_different_process",
    timeout = "moderate",
    srcs = [
        "test_fixture.py",
        "test_setter_and_getter_different_process.py",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "test_setter_and_getter_same_process",
    timeout = "moderate",
    srcs = [
        "test_fixture.py",
        "test_setter_and_getter_same_process.py",
    ],
    filesystem = ":same_process_filesystem",
)
