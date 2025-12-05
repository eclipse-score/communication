# *******************************************************************************
# Copyright (c) 2024 Contributors to the Eclipse Foundation
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

load("@//third_party/traceability/bazel:safety_analysis.bzl", "safety_analysis")
load("@//third_party/traceability/tools/source_code_linker:collect_source_files.bzl", "parse_source_files_for_links")
load("@trlc//:trlc.bzl", "trlc_requirements", "trlc_requirements_test")

# FMEA
trlc_requirements(
    name = "sample_fmea_failure_modes",
    srcs = [
        "sample_fmea_failure_modes.trlc",
    ],
    spec = [
        "@//third_party/traceability/config/trlc/score_model:requirement_metamodel",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements(
    name = "sample_fmea_control_measures",
    srcs = [
        "sample_fmea_control_measures.trlc",
    ],
    spec = [
        "@//third_party/traceability/config/trlc/score_model:requirement_metamodel",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "sample_fmea_test",
    reqs = [
        ":sample_fmea_failure_modes",
        ":sample_fmea_control_measures",
    ],
    visibility = ["//visibility:public"],
)

#  FTA

filegroup(
    name = "sample_fta",
    srcs = [
        "sample_fta.puml",
    ],
    visibility = ["//visibility:public"],
)

parse_source_files_for_links(
    name = "fta_toplevel",
    srcs_and_deps = [":sample_fta"],
    trace = "plantuml_alias_cpp",
    trace_tags = ["$TopEvent"],
)

parse_source_files_for_links(
    name = "fta_basic",
    srcs_and_deps = [":sample_fta"],
    trace_tags = ["$BasicEvent"],
)

safety_analysis(
    name = "sample_safety_analysis",
    controlmeasures = [":sample_fmea_control_measures"],
    failuremodes = [":sample_fmea_failure_modes"],
    fta = [":sample_fta"],
    public_api = ["@//third_party/traceability/doc/sample_library/design:public_api"],
)
