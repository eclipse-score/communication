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
package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "json_schema_validator_lib",
    srcs = glob(["src/*"]),
    hdrs = ["src/nlohmann/json-schema.hpp"],
    features = ["third_party_warnings"],
    includes = ["src"],
    deps = ["@nlohmann_json//:json"],
)

cc_binary(
    name = "json_schema_validator",
    srcs = ["app/json-schema-validate.cpp"],
    deps = [":json_schema_validator_lib"],
)