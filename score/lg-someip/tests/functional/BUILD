# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_filegroup", "pkg_files")
load("//score/lg-someip:lgsomeip.bzl", "lgsomeip_cc_binary")

# Wire-level functional tests of the lg-someip stack: test apps on the lgsomeip API inside a docker target, driven
# from the host by the pytest suites in test_cases.

[
    lgsomeip_cc_binary(
        name = APP + "_app",
        srcs = ["apps/{0}/{0}_app.cpp".format(APP)],
        deps = ["//score/lg-someip/src/someip:lgsomeip"],
    )
    for APP in [
        "ets",
        "tc8",
    ]
]

# One image filesystem per suite family: /opt/someip_test/bin holds the routing daemon and the family's test app,
# /opt/someip_test/etc its config.
[
    (
        pkg_files(
            name = APP + "_bin",
            srcs = [
                ":" + APP + "_app",
                "//score/lg-someip/daemon:someip-daemon",
            ],
            attributes = pkg_attributes(mode = "0755"),
            prefix = "/opt/someip_test/bin",
        ),
        pkg_files(
            name = APP + "_etc",
            srcs = ["config/" + APP + "_lgsomeip_config.json"],
            prefix = "/opt/someip_test/etc",
        ),
        pkg_filegroup(
            name = APP + "_filesystem",
            srcs = [
                ":" + APP + "_bin",
                ":" + APP + "_etc",
            ],
            visibility = ["//score/lg-someip/tests/functional:__subpackages__"],
        ),
    )
    for APP in [
        "ets",
        "tc8",
    ]
]
