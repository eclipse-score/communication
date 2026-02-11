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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "lola_interface",
    srcs = ["lola_interface.cpp"],
    hdrs = ["lola_interface.h"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/performance_benchmarks/api_microbenchmarks:__pkg__"],
    deps = [
        "//score/mw/com",
    ],
)

cc_binary(
    name = "lola_public_api_benchmarks",
    srcs = [
        "lola_public_api_benchmarks.cpp",
    ],
    data = [
        "//score/mw/com/performance_benchmarks/api_microbenchmarks/config:basic_config",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["benchmark"],
    deps = [
        "//score/mw/com",
        "@google_benchmark//:benchmark_main",
        "@score_baselibs//score/language/futurecpp",
        "@score_logging//score/mw/log",
    ],
)

cc_binary(
    name = "lola_get_num_new_samples_available_benchmark",
    srcs = [
        "lola_get_num_new_samples_benchmarks.cpp",
    ],
    data = [
        "//score/mw/com/performance_benchmarks/api_microbenchmarks/config:basic_config",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["benchmark"],
    deps = [
        ":lola_interface",
        "//score/mw/com",
        "@google_benchmark//:benchmark_main",
        "@score_baselibs//score/language/futurecpp",
        "@score_logging//score/mw/log",
    ],
)

cc_binary(
    name = "lola_get_new_samples_benchmark",
    srcs = [
        "lola_get_new_samples_benchmarks.cpp",
    ],
    data = [
        "//score/mw/com/performance_benchmarks/api_microbenchmarks/config:config_qm_high_frequency_send_large_data",
        "//score/mw/com/performance_benchmarks/api_microbenchmarks/config:logging_json",
    ],
    env = {"MW_LOG_CONFIG_FILE": "$(location //score/mw/com/performance_benchmarks/api_microbenchmarks/config:logging_json)"},
    features = COMPILER_WARNING_FEATURES,
    tags = ["benchmark"],
    deps = [
        ":lola_interface",
        "//score/mw/com",
        "@google_benchmark//:benchmark_main",
        "@score_baselibs//score/language/futurecpp",
        "@score_logging//score/mw/log",
    ],
)
