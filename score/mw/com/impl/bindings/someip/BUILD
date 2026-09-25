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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "element_fq_id",
    srcs = ["element_fq_id.cpp"],
    hdrs = ["element_fq_id.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:service_element_type",
        "//score/mw/com/impl/configuration",
    ],
)

cc_library(
    name = "event_data_storage",
    srcs = ["event_data_storage.cpp"],
    hdrs = ["event_data_storage.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:initialize_sample_callback",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory:data_type_size_info",
    ],
)

cc_library(
    name = "transport",
    srcs = ["in_process_transport.cpp"],
    hdrs = [
        "i_transport.h",
        "in_process_transport.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":element_fq_id",
        "//score/mw/com/impl:error",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "slot_allocation_control",
    srcs = ["slot_allocation_control.cpp"],
    hdrs = ["slot_allocation_control.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [":event_data_storage"],
)

cc_library(
    name = "skeleton_event_properties",
    hdrs = ["skeleton_event_properties.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
)

cc_library(
    name = "sample_allocatee_ptr",
    srcs = ["sample_allocatee_ptr.cpp"],
    hdrs = ["sample_allocatee_ptr.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":event_data_storage",
        ":slot_allocation_control",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton",
    srcs = ["skeleton.cpp"],
    hdrs = ["skeleton.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":element_fq_id",
        ":event_data_storage",
        ":transport",
        "//score/mw/com/impl:binding_type",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:initialize_sample_callback",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:skeleton_binding",
        "//score/mw/com/impl:skeleton_event_binding",
        "//score/mw/com/impl/configuration",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "skeleton_event",
    srcs = ["skeleton_event.cpp"],
    hdrs = ["skeleton_event.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":element_fq_id",
        ":event_data_storage",
        ":sample_allocatee_ptr",
        ":skeleton",
        ":skeleton_event_properties",
        ":slot_allocation_control",
        "//score/mw/com/impl:binding_type",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:initialize_sample_callback",
        "//score/mw/com/impl:skeleton_event_binding",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "//score/mw/com/impl/tracing:skeleton_event_tracing_data",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "runtime",
    srcs = ["service_discovery_client.cpp"],
    hdrs = [
        "runtime.h",
        "service_discovery_client.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:binding_type",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:i_service_discovery_client",
        "//score/mw/com/impl:runtime_interfaces",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "someip",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":element_fq_id",
        ":event_data_storage",
        ":runtime",
        ":sample_allocatee_ptr",
        ":skeleton",
        ":skeleton_event",
        ":slot_allocation_control",
        ":transport",
    ],
)
