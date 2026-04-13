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
        "//score/mw/com/test/methods/edge_cases_test:edge-case-test-pkg",
    ],
)

py_library(
    name = "test_fixture",
    srcs = ["test_fixture.py"],
)

integration_test(
    name = "disabled_method_test",
    srcs = [
        "disabled_method_test.py",
        ":test_fixture",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "incomplete_handlers_test",
    srcs = [
        "incomplete_handlers_test.py",
        ":test_fixture",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "proxy_recreation_test",
    srcs = [
        "proxy_recreation_test.py",
        ":test_fixture",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "skeleton_recreation_test",
    srcs = [
        "skeleton_recreation_test.py",
        ":test_fixture",
    ],
    filesystem = ":filesystem",
)

test_suite(
    name = "component_tests",
    tests = [
        "disabled_method_test",
        "incomplete_handlers_test",
        "proxy_recreation_test",
        "skeleton_recreation_test",
    ],
)
