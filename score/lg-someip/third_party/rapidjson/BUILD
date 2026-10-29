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

# Header-only RapidJSON (Tencent/rapidjson, commit f54b0e47a08782a6131cc3d60f94d038fa6e0a51,
# the same one the CMake build fetches). Vendored so that the Bazel build needs no
# change to the root MODULE.bazel. See LICENSE.
cc_library(
    name = "rapidjson",
    hdrs = glob(["include/rapidjson/**/*.h"]),
    includes = ["include"],
    visibility = ["//score/lg-someip:__subpackages__"],
)
