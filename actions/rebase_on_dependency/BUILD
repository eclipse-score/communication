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

load("@rules_python//python:defs.bzl", "py_library", "py_test")

py_library(
    name = "find_dependencies_lib",
    srcs = ["scripts/find_dependencies.py"],
    imports = ["scripts"],
)

py_library(
    name = "rebase_on_prs_lib",
    srcs = ["scripts/rebase_on_prs.py"],
    imports = ["scripts"],
)

py_test(
    name = "find_dependencies_py_test",
    srcs = ["test/find_dependencies_test.py"],
    main = "test/find_dependencies_test.py",
    deps = [":find_dependencies_lib"],
)

py_test(
    name = "rebase_on_prs_py_test",
    srcs = ["test/rebase_on_prs_test.py"],
    main = "test/rebase_on_prs_test.py",
    deps = [":rebase_on_prs_lib"],
)
