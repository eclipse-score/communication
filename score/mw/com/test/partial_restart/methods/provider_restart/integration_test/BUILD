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

load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("//quality/integration_testing:integration_testing.bzl", "integration_test")

pkg_tar(
    name = "filesystem",
    deps = [
        "//score/mw/com/test/partial_restart/methods/provider_restart:provider_restart-pkg",
    ],
)

integration_test(
    name = "partial_restart_methods_provider_graceful_test",
    srcs = [
        "partial_restart_methods_provider_graceful_test.py",
        "partial_restart_methods_provider_test_fixture.py",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "partial_restart_methods_provider_graceful_no_proxy_test",
    srcs = [
        "partial_restart_methods_provider_graceful_no_proxy_test.py",
        "partial_restart_methods_provider_test_fixture.py",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "partial_restart_methods_provider_kill_test",
    srcs = [
        "partial_restart_methods_provider_kill_test.py",
        "partial_restart_methods_provider_test_fixture.py",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "partial_restart_methods_provider_kill_no_proxy_test",
    srcs = [
        "partial_restart_methods_provider_kill_no_proxy_test.py",
        "partial_restart_methods_provider_test_fixture.py",
    ],
    filesystem = ":filesystem",
)

test_suite(
    name = "component_tests",
    tests = [
        ":partial_restart_methods_provider_graceful_test",
        ":partial_restart_methods_provider_graceful_no_proxy_test",
        ":partial_restart_methods_provider_kill_test",
        ":partial_restart_methods_provider_kill_no_proxy_test",
    ],
    visibility = ["//score/mw/com/test/partial_restart/methods/provider_restart:__subpackages__"],
)
