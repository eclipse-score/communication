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
load("@score_format_checker//:macros.bzl", "use_format_targets")
load("@score_starpls_lsp//:starpls.bzl", "setup_starpls")

# Add target for formatting checks
use_format_targets()

# Add StarPLS server for formatting
setup_starpls(
    name = "starpls_server",
    visibility = ["//visibility:public"],
)
