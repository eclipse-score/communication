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
    "safety_analysis",
)

filegroup(
    name = "fta_files",
    srcs = [
        "fta_connection_context_data_wrong.puml",
        "fta_ipc_api_misuse_or_lifecycle_violation.puml",
        "fta_ipc_channel_unavailable.puml",
        "fta_message_not_delivered_correctly.puml",
        "fta_message_timing_violated.puml",
        "fta_notification_not_delivered.puml",
        "fta_server_message_handling_failure.puml",
        "fta_state_machine_error.puml",
    ],
    visibility = ["//score/message_passing/dependability:__pkg__"],
)

safety_analysis(
    name = "message_passing_safety_analysis",
    arch_design = "//score/message_passing/dependability/software_architectural_design:message_passing_architectural_design",
    failuremodes = ["failure_modes.trlc"],
    fta_package = "message_passing_fta",
    root_causes = [":fta_files"],
    safetymeasures = ["safetymeasures.trlc"],
    visibility = ["//score/message_passing/dependability:__pkg__"],
)

dependability_analysis(
    name = "message_passing_dependability_analysis",
    arch_design = "//score/message_passing/dependability/software_architectural_design:message_passing_architectural_design",
    safety_analysis = ["//score/message_passing/dependability/safety_analysis:message_passing_safety_analysis"],
    tags = ["manual"],
    visibility = ["//score/message_passing/dependability:__pkg__"],
)
