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

filegroup(
    name = "design",
    srcs = [
        "readme.rst",
        ":unit_design",
    ],
    visibility = ["//docs:__pkg__"],
)

filegroup(
    name = "unit_design",
    srcs = [
        "static_design.puml",
    ],
    visibility = ["@//visibility:public"],
)

filegroup(
    name = "public_api",
    srcs = [
        "public_api.puml",
    ],
    visibility = ["@//visibility:public"],
)
