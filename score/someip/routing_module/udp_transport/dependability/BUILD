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
    name = "udp_transport_unit_design",
    dynamic = [
        "linux_transport_sequence.puml",
        "udp_transport_receive_sequence.puml",
    ],
    static = [
        "udp_transport_class.puml",
        "udp_transport_class.rst",
        "linux_transport.rst",
    ],
    visibility = ["//score/someip/routing_module/udp_transport:__pkg__"],
)
