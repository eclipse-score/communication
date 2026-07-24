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

load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

py_library(
    name = "dashboard_lib",
    srcs = ["generate_dashboard.py"],
    data = ["dashboard.html.j2"],
    deps = [
        "@score_communication_pip//jinja2",
        "@score_communication_pip//markupsafe",
    ],
)

py_binary(
    name = "generate_dashboard",
    srcs = ["generate_dashboard.py"],
    main = "generate_dashboard.py",
    visibility = ["//visibility:private"],
    deps = [":dashboard_lib"],
)

py_test(
    name = "generate_dashboard_test",
    srcs = ["generate_dashboard_test.py"],
    deps = [":dashboard_lib"],
)
