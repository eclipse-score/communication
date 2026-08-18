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

load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "provided_service",
    srcs = ["provided_service.cpp"],
    hdrs = ["provided_service.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "provided_service_container",
    srcs = ["provided_service_container.cpp"],
    hdrs = ["provided_service_container.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":provided_service",
    ],
)

cc_library(
    name = "proxy_future",
    srcs = ["proxy_future.cpp"],
    hdrs = ["proxy_future.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        "@score_baselibs//score/concurrency/future",
    ],
)

cc_library(
    name = "service",
    srcs = ["proxy_needs.cpp"],
    hdrs = ["proxy_needs.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":proxy_future",
    ],
)

cc_library(
    name = "factory",
    srcs = ["proxy_needs_factory.cpp"],
    hdrs = ["proxy_needs_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":service",
    ],
)

cc_library(
    name = "proxy_data",
    srcs = ["proxy_data.cpp"],
    hdrs = ["proxy_data.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":service",
    ],
)
