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

integration_test(
    name = "semi_dynamic_methods_same_process_test",
    srcs = ["semi_dynamic_methods_same_process_test.py"],
    filesystem = "//score/mw/com/test/methods/semi_dynamic_methods:main_consumer_and_provider-pkg",
    service_discovery_daemon = True,
)

pkg_filegroup(
    name = "different_processes_filesystem",
    srcs = [
        "//score/mw/com/test/methods/semi_dynamic_methods:main_consumer-pkg",
        "//score/mw/com/test/methods/semi_dynamic_methods:main_provider-pkg",
    ],
)

integration_test(
    name = "semi_dynamic_methods_different_processes_test",
    srcs = ["semi_dynamic_methods_different_processes_test.py"],
    filesystem = ":different_processes_filesystem",
    service_discovery_daemon = True,
)
