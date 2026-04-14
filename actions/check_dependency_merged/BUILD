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
    name = "check_dependencies_merged_lib",
    srcs = ["scripts/check_dependencies_merged.py"],
    imports = ["scripts"],
)

py_test(
    name = "check_dependencies_merged_py_test",
    srcs = ["test/check_dependencies_merged_test.py"],
    main = "test/check_dependencies_merged_test.py",
    deps = [":check_dependencies_merged_lib"],
)

