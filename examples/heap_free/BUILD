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

# Common header sets used by all four heap-free example binaries.
cc_library(
    name = "heap_check_lib",
    hdrs = ["heap_check/heap_check.h"],
    includes = ["."],
    visibility = ["//:__subpackages__"],
)

cc_library(
    name = "sensor_interface",
    hdrs = ["common/sensor_interface.h"],
    includes = ["."],
    visibility = ["//:__subpackages__"],
    deps = ["@score_communication//score/mw/com"],
)
