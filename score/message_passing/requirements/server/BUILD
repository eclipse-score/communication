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

load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "unit",
)

# Unit: message passing server
# Covers the platform-specific server implementations (QNX dispatch and Unix domain)
unit(
    name = "server",
    unit_design = ["//score/message_passing/design:server_unit_design"],
    implementation = [
        "//score/message_passing:message_passing_qnx_dispatch",
        "//score/message_passing:message_passing_unix_domain",
    ],
    scope = [
        "//score/message_passing:message_passing_qnx_dispatch",
        "//score/message_passing:message_passing_unix_domain",
    ],
    tests = [
        "//score/message_passing:qnx_dispatch_test",
        "//score/message_passing:unix_domain_test",
    ],
    visibility = ["//visibility:public"],
)
