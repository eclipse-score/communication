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

fmea(
    name = "service_discovery_fmea",
    arch_design = "//score/mw/com/service_discovery/dependability/software_architectural_design:service_discovery_architectural_design",
    controlmeasures = ["control_measures.trlc"],
    failuremodes = ["failure_modes.trlc"],
    visibility = ["//score/mw/com/service_discovery/dependability:__subpackages__"],
)

dependability_analysis(
    name = "service_discovery_dependability_analysis",
    arch_design = "//score/mw/com/service_discovery/dependability/software_architectural_design:service_discovery_architectural_design",
    fmea = [":service_discovery_fmea"],
    tags = ["manual"],
    visibility = ["//score/mw/com/service_discovery/dependability:__subpackages__"],
)
