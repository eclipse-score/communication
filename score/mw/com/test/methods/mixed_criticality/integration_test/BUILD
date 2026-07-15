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
        "//score/mw/com/test/methods/mixed_criticality:consumer-pkg",
        "//score/mw/com/test/methods/mixed_criticality:provider-pkg",
    ],
)

integration_test(
    name = "qm_consumer_qm_provider_test",
    srcs = [
        "qm_consumer_qm_provider_test.py",
        "test_fixture.py",
    ],
    filesystem = ":filesystem",
    service_discovery_daemon = True,
)

integration_test(
    name = "qm_consumer_asilb_provider_test",
    srcs = [
        "qm_consumer_asilb_provider_test.py",
        "test_fixture.py",
    ],
    filesystem = ":filesystem",
    service_discovery_daemon = True,
)

integration_test(
    name = "asilb_consumer_qm_provider_test",
    srcs = [
        "asilb_consumer_qm_provider_test.py",
        "test_fixture.py",
    ],
    filesystem = ":filesystem",
    service_discovery_daemon = True,
)

integration_test(
    name = "asilb_consumer_asilb_provider_test",
    srcs = [
        "asilb_consumer_asilb_provider_test.py",
        "test_fixture.py",
    ],
    filesystem = ":filesystem",
    service_discovery_daemon = True,
)
