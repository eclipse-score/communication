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
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "unit_design")

unit_design(
    name = "iosock_transport_unit_design",
    dynamic = [
        "iosock_transport_receive_sequence.puml",
    ],
    static = [
        "iosock_transport_class.puml",
        "iosock_transport_class.rst",
    ],
    visibility = ["//score/someip/routing_module/iosock_transport:__pkg__"],
)
