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

# Unit: message passing client
# Covers the client-side connection management and common messaging infrastructure
unit(
    name = "client",
    unit_design = ["//score/message_passing/design:client_unit_design"],
    implementation = [
        "//score/message_passing:message_passing_common",
    ],
    scope = [
        "//score/message_passing:message_passing_common",
    ],
    tests = [
        "//score/message_passing:client_connection_test",
    ],
    visibility = ["//visibility:public"],
)
