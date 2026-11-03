# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

load("@score_tooling//cr_checker:cr_checker.bzl", "copyright_checker")

filegroup(
    name = "templates",
    srcs = [
        "templates.ini",
    ],
    visibility = ["//:__pkg__"],
)

filegroup(
    name = "config",
    srcs = [
        "config.json",
    ],
    visibility = ["//:__pkg__"],
)

# Restricted to the issue template that the release postprocessing workflow rewrites.
# TODO Shall be removed once score_tooling is updated to v3.0.2 with the fixes to the copyright checker.
# See: https://github.com/eclipse-score/communication/issues/1540
copyright_checker(
    name = "issue_template_copyright",
    srcs = [".github/ISSUE_TEMPLATE/1-bugfix.yml"],
    config = ":config",
    template = ":templates",
    visibility = ["//visibility:private"],
)
