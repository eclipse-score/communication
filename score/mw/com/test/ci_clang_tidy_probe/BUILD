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

load("@rules_cc//cc:defs.bzl", "cc_library")

# Temporary probe used to verify that the CI clang-tidy lint job still gates
# pull requests as expected. This target intentionally contains a
# readability-magic-numbers violation (see probe.cpp). Remove this whole
# directory once the CI validation is complete.
cc_library(
    name = "ci_clang_tidy_probe",
    srcs = ["probe.cpp"],
    hdrs = ["probe.h"],
    visibility = ["//score/mw/com:__subpackages__"],
)
