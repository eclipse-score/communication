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
cc_binary(
    name = "OffsetPtrBenchmark",
    srcs = ["offset_ptr_benchmark.cpp"],
    visibility = [
        # "@ddad@score-baselibs//score/memory/shared/test:__subpackages__",
    ],
    deps = ["@score-baselibs//score/memory/shared"] + [
        "@score-baselibs//score/mw/log",
        "//third_party/boost:program_options",
    ],
)
