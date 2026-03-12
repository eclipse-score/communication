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

load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

# Re-package suppressions from @score_cpp_policies for integration testing
pkg_tar(
    name = "suppressions_pkg",
    srcs = ["@score_cpp_policies//sanitizers:suppressions"],
    mode = "0444",
    package_dir = "sanitizers/suppressions",
    visibility = ["//visibility:public"],
)
