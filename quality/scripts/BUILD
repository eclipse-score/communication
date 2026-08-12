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
    name = "flaky_reports_lib",
    srcs = [
        "__init__.py",
        "collect_flaky_tests.py",
        "merge_flaky_reports.py",
        "sync_flaky_issues.py",
    ],
)

py_binary(
    name = "collect_flaky_tests",
    srcs = ["collect_flaky_tests.py"],
    main = "collect_flaky_tests.py",
    deps = [":flaky_reports_lib"],
)

py_binary(
    name = "merge_flaky_reports",
    srcs = ["merge_flaky_reports.py"],
    main = "merge_flaky_reports.py",
    deps = [":flaky_reports_lib"],
)

py_binary(
    name = "sync_flaky_issues",
    srcs = ["sync_flaky_issues.py"],
    main = "sync_flaky_issues.py",
    deps = [":flaky_reports_lib"],
)

py_test(
    name = "flaky_reports_test",
    srcs = ["flaky_reports_test.py"],
    deps = [":flaky_reports_lib"],
)
