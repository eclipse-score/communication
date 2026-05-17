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
        "//score/mw/com/test/methods/signature_variations:main_consumer-pkg",
        "//score/mw/com/test/methods/signature_variations:main_provider-pkg",
    ],
)

integration_test(
    name = "signature_variations_test",
    srcs = [
        "signature_variations_test.py",
    ],
    filesystem = ":filesystem",
)

test_suite(
    name = "component_tests",
    tests = [
        ":signature_variations_test",
    ],
)
