# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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
cc_library(
    name = "typedmemory_mock",
    testonly = True,
    srcs = ["typed_memory_mock.cpp"],
    hdrs = ["typed_memory_mock.h"],
    visibility = ["@score_baselibs//score/memory/shared:__pkg__"],
    deps = [
        "@googletest//:gtest",
        "@score_baselibs//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
    ],
)
