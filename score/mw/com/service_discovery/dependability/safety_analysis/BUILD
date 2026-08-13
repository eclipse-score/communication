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
        "fta_missed_service_discovery_notification.puml",
        "fta_registration_integrity_escalation.puml",
        "fta_stale_registry_entry.puml",
        "fta_stale_subscription_after_disconnect.puml",
    ],
    visibility = ["//score/mw/com/service_discovery/dependability:__subpackages__"],
)

fmea(
    name = "service_discovery_fmea",
    arch_design = "//score/mw/com/service_discovery/dependability/software_architectural_design:service_discovery_architectural_design",
    controlmeasures = ["control_measures.trlc"],
    failuremodes = ["failure_modes.trlc"],
    root_causes = [":fta_files"],
    visibility = ["//score/mw/com/service_discovery/dependability:__subpackages__"],
)

dependability_analysis(
    name = "service_discovery_dependability_analysis",
    arch_design = "//score/mw/com/service_discovery/dependability/software_architectural_design:service_discovery_architectural_design",
    fmea = [":service_discovery_fmea"],
    tags = ["manual"],
    visibility = ["//score/mw/com/service_discovery/dependability:__subpackages__"],
)
