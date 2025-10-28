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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_binary(
    name = "lola_public_api_benchmarks",
    srcs = [
        "lola_public_api_benchmarks.cpp",
    ],
    data = [
        "//score/mw/com/performance_benchmarks/api_microbenchmarks/config",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["benchmark"],
    deps = [
        "//score/mw/com",
        "@google_benchmark//:benchmark_main",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
    ],
)
