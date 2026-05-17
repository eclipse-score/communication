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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "tracing_runtime",
    srcs = ["tracing_runtime.cpp"],
    hdrs = ["tracing_runtime.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__subpackages__",
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:service_element_type",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/tracing:i_binding_tracing_runtime",
        "//score/mw/com/impl/tracing:service_element_tracing_data",
        "//score/mw/com/impl/tracing/configuration:service_element_instance_identifier_view",
        "@score_baselibs//score/containers:dynamic_array",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/safecpp/safe_math",
        "@score_baselibs//score/language/safecpp/scoped_function:move_only_scoped_function",
        "@score_baselibs//score/memory/shared:i_shared_memory_resource",
        "@score_baselibs//score/mw/log",
    ],
)

cc_unit_test(
    name = "tracing_runtime_test",
    srcs = ["tracing_runtime_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":tracing_runtime",
        "//score/mw/com/impl/bindings/lola/test:skeleton_test_resources",
        "@score_baselibs//score/analysis/tracing/generic_trace_library/mock:trace_library_mock",
    ],
)
