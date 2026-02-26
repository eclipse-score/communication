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
    hdrs = ["provided_service.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "provided_service_container",
    hdrs = ["provided_service_container.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "proxy_future",
    hdrs = ["proxy_future.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "service",
    hdrs = ["proxy_needs.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":proxy_future",
    ],
)

cc_library(
    name = "factory",
    hdrs = ["proxy_needs_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":service",
    ],
)
