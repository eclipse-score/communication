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
    name = "filesystem_different_process",
    deps = [
        "//score/mw/com/test/methods/multiple_proxies:consumer0-pkg",
        "//score/mw/com/test/methods/multiple_proxies:consumer1-pkg",
        "//score/mw/com/test/methods/multiple_proxies:consumer2-pkg",
        "//score/mw/com/test/methods/multiple_proxies:consumer3-pkg",
        "//score/mw/com/test/methods/multiple_proxies:provider-pkg",
    ],
)

pkg_tar(
    name = "filesystem_same_process",
    deps = [
        "//score/mw/com/test/methods/multiple_proxies:combined-provider-consumer-pkg",
    ],
)

integration_test(
    name = "different_process_provider_and_consumers",
    timeout = "moderate",
    srcs = [
        "test_different_process_provider_and_consumers.py",
    ],
    filesystem = ":filesystem_different_process",
)

integration_test(
    name = "same_process_provider_and_consumers",
    timeout = "moderate",
    srcs = [
        "test_same_process_provider_and_consumers.py",
    ],
    filesystem = ":filesystem_same_process",
)



test_suite(
    name = "component_tests",
    tests = [
        ":different_process_provider_and_consumers",
        ":same_process_provider_and_consumers",
    ],
)
