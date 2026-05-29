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
        "//score/mw/com/test/methods/edge_cases_test:edge-case-test-pkg",
    ],
)

integration_test(
    name = "disabled_method_test",
    srcs = [
        "disabled_method_test.py",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "incomplete_handlers_test",
    srcs = [
        "incomplete_handlers_test.py",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "proxy_recreation_test",
    srcs = [
        "proxy_recreation_test.py",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "skeleton_recreation_test",
    srcs = [
        "skeleton_recreation_test.py",
    ],
    filesystem = ":filesystem",
)
