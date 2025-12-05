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
load("@lobster//:lobster.bzl", "lobster_trlc")
load("@trlc//:trlc.bzl", "trlc_requirements", "trlc_requirements_test")
load("//third_party/traceability/tools/source_code_linker:collect_source_files.bzl", "parse_source_files_for_links")

# FMEA
trlc_requirements(
    name = "fmea_ipc",
    srcs = [
        "fmea_ipc.trlc",
        "@//score/mw/com/requirements/component_requirements:component_requirements_ipc",
    ],
    spec = [
        "@//third_party/traceability/config/trlc/score_model:requirement_metamodel",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "fmea_ipc_test",
    reqs = [
        ":fmea_ipc",
    ],
    visibility = ["//visibility:public"],
)

lobster_trlc(
    name = "fmea_ipc_lobster",
    config = "@//third_party/traceability/config/lobster:safety_analysis_conf",
    requirements = [
        ":fmea_ipc",
    ],
    visibility = ["//visibility:public"],
)

#  FTA

filegroup(
    name = "fta",
    srcs = [
        "fta.puml",
    ],
    visibility = ["//visibility:public"],
)

parse_source_files_for_links(
    name = "fta_tags",
    srcs_and_deps = [":fta"],
)

safety_analysis(
    name = "mw_com_safety_analysis",
    controlmeasures = [":aou_ipc"],
    failuremodes = [":fmea_ipc"],
    fta = [":fta"],
    public_api = ["@//third_party/traceability/doc/sample_library/design:public_api"],
)

# AoUs
trlc_requirements(
    name = "aou_ipc",
    srcs = [
        "aou_ipc.trlc",
    ],
    spec = [
        "@//third_party/traceability/config/trlc/score_model:requirement_metamodel",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "aou_ipc_test",
    reqs = [
        ":aou_ipc",
    ],
    visibility = ["//visibility:public"],
)

lobster_trlc(
    name = "aou_ipc_lobster",
    config = "@//third_party/traceability/config/lobster:safety_analysis_conf",
    requirements = [
        ":aou_ipc",
    ],
    visibility = ["//visibility:public"],
)
