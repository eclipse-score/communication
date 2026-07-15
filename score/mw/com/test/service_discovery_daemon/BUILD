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

load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

exports_files(
    ["logging.json"],
    visibility = ["//score/mw/com/test:__subpackages__"],
)

cc_binary(
    name = "service_discovery_daemon_app",
    srcs = ["service_discovery_daemon_application.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com/service_discovery:service_discovery_daemon",
        "//score/mw/com/service_discovery:service_discovery_message_passing_server",
        "//score/mw/com/service_discovery:service_discovery_registry",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
    ],
)

pkg_application(
    name = "service_discovery_daemon-pkg",
    app_name = "ServiceDiscoveryDaemonApp",
    bin = [":service_discovery_daemon_app"],
    etc = ["logging.json"],
    visibility = [
        "//score/mw/com/test:__subpackages__",
    ],
)
