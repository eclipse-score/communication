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
    name = "provided_service_builder",
    hdrs = ["provided_service_builder.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":provided_service_decorator",
        ":provided_services",
        "//score/mw/service:provided_service_container",
    ],
)

cc_library(
    name = "provided_service_decorator",
    hdrs = ["provided_service_decorator.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "provided_services",
    hdrs = ["provided_services.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "single_instantiation_strategy",
    hdrs = ["single_instantiation_strategy.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
)
