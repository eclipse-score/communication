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

load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")

filegroup(
    name = "joined_benchmark_config_json",
    srcs = ["joined_benchmark_config.json"],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__subpackages__"],
)

validate_json_schema_test(
    name = "validate_lola_schema",
    json = "//score/mw/com/performance_benchmarks/macro_benchmark/config:mw_com_config_json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "validate_joined_benchmark_config_schema",
    json = "joined_benchmark_config.json",
    schema = "joined_benchmark_config_schema.json",
    tags = ["lint"],
)

filegroup(
    name = "mw_com_config_json",
    srcs = ["mw_com_config.json"],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__subpackages__"],
)

filegroup(
    name = "logging_json",
    srcs = ["logging.json"],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__subpackages__"],
)
