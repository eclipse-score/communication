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
load("@codeql_coding_standards_pip_hub//:requirements.bzl", "requirement")

py_binary(
    name = "process_coding_standards_config",
    srcs = ["scripts/configuration/process_coding_standards_config.py"],
    deps = [requirement("pyyaml")],
    visibility = ["//visibility:public"],
)

py_binary(
    name = "analysis_report",
    srcs = ["scripts/reports/analysis_report.py",
            "scripts/reports/diagnostics.py" ,
            "scripts/reports/deviations.py",
            "scripts/reports/error.py",
            "scripts/reports/codeqlvalidation.py",
            "scripts/reports/utils.py",
            "scripts/shared/codeql.py",
            "scripts/reports/guideline_recategorizations.py"],
    data = ["supported_codeql_configs.json"] + glob(["cpp/**"]),
    deps = [requirement("pyyaml")],
    imports = ["scripts/reports","scripts/shared"],
    visibility = ["//visibility:public"],
)
