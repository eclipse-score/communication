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

load("@rules_python//python/entry_points:py_console_script_binary.bzl", "py_console_script_binary")

# Makes tools/lint/ a Bazel package so linters.bzl can be referenced as a
# label target (e.g. //tools/lint:linters.bzl%clang_tidy) in bazelrc.
exports_files([
    "linters.bzl",
])

# Pylint executable used by the pylint lint aspect (see linters.bzl).
py_console_script_binary(
    name = "pylint",
    pkg = "@score_communication_pip//pylint:pkg",
    visibility = ["//:__pkg__"],
)
