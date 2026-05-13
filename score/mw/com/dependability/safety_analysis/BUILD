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

load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "fmea",
)

# Root Causes
filegroup(
    name = "root_causes",
    srcs = [
        "//score/mw/com/dependability/safety_analysis/root_causes:general",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy:root_causes_proxy",
        "//score/mw/com/dependability/safety_analysis/root_causes/skeleton:root_causes_skeleton",
        "//score/mw/com/dependability/safety_analysis/root_causes/smart_pointer:smart_pointer_fta",
    ],
    visibility = ["//visibility:public"],
)

fmea(
    name = "mw_com_fmea",
    arch_design = "//score/mw/com/dependability/software_architectural_design:software_architectural_design",
    controlmeasures = ["aou.trlc"],
    failuremodes = ["failure_modes.trlc"],
    root_causes = [":root_causes"],
    visibility = ["//visibility:public"],
)
