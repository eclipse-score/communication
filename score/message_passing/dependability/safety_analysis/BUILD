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

load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "dependability_analysis",
    "fmea",
)

filegroup(
    name = "fta_files",
    srcs = [
        "fta_client_connection_failed.puml",
        "fta_server_not_listening.puml",
    ],
    visibility = ["//score/message_passing/dependability:__pkg__"],
)

fmea(
    name = "message_passing_fmea",
    arch_design = "//score/message_passing/dependability/software_architectural_design:message_passing_architectural_design",
    controlmeasures = ["control_measures.trlc"],
    failuremodes = ["failure_modes.trlc"],
    root_causes = [":fta_files"],
    visibility = ["//score/message_passing/dependability:__pkg__"],
)

dependability_analysis(
    name = "message_passing_dependability_analysis",
    arch_design = "//score/message_passing/dependability/software_architectural_design:message_passing_architectural_design",
    fmea = ["//score/message_passing/dependability/safety_analysis:message_passing_fmea"],
    tags = ["manual"],
    visibility = ["//score/message_passing/dependability:__pkg__"],
)
