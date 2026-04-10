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
        "//score/mw/com/test/methods/mixed_criticality:consumer-pkg",
        "//score/mw/com/test/methods/mixed_criticality:provider-pkg",
    ],
)

py_library(
    name = "test_fixture",
    srcs = ["test_fixture.py"],
)

integration_test(
    name = "qm_consumer_qm_provider_test",
    srcs = [
        "qm_consumer_qm_provider_test.py",
        ":test_fixture",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "qm_consumer_asilb_provider_test",
    srcs = [
        "qm_consumer_asilb_provider_test.py",
        ":test_fixture",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "asilb_consumer_qm_provider_test",
    srcs = [
        "asilb_consumer_qm_provider_test.py",
        ":test_fixture",
    ],
    filesystem = ":filesystem",
)

integration_test(
    name = "asilb_consumer_asilb_provider_test",
    srcs = [
        "asilb_consumer_asilb_provider_test.py",
        ":test_fixture",
    ],
    filesystem = ":filesystem",
)


test_suite(
    name = "component_tests",
    tests = [
        ":asilb_consumer_asilb_provider_test",
        ":asilb_consumer_qm_provider_test",
        ":qm_consumer_asilb_provider_test",
        ":qm_consumer_qm_provider_test",
    ],
)
