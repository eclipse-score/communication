# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "component", "unit")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

component(
    name = "someip_component",
    components = [
        ":someip_runtime",
        ":someip_proxy",
        ":someip_skeleton",
    ],
    requirements = [],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
    visibility = ["//score/mw/com/impl/bindings:__pkg__"],
)

unit(
    name = "someip_runtime",
    scope = [],
    tests = [],
    unit_design = [],
    implementation = [":runtime"],
)

unit(
    name = "someip_proxy",
    scope = [],
    tests = [],
    unit_design = [],
    implementation = [":proxy"],
)

unit(
    name = "someip_skeleton",
    scope = [],
    tests = [],
    unit_design = [],
    implementation = [":skeleton"],
)

cc_library(
    name = "someip",
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        ":event_binding_factory",
        ":proxy_binding_factory",
        ":proxy_event",
        ":proxy",
        ":runtime",
        ":skeleton_binding_factory",
        ":skeleton_event",
        ":skeleton",
    ],
)

cc_library(
    name = "runtime",
    srcs = ["runtime.cpp"],
    hdrs = ["runtime.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:i_service_discovery_client",
        "//score/mw/com/impl:runtime_interfaces",
    ],
)

cc_library(
    name = "proxy",
    srcs = ["proxy.cpp"],
    hdrs = ["proxy.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        "//score/mw/com/impl:proxy_binding",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_event",
    srcs = ["proxy_event.cpp"],
    hdrs = ["proxy_event.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:proxy_event_binding",
    ],
)

cc_library(
    name = "proxy_binding_factory",
    srcs = ["proxy_binding_factory.cpp"],
    hdrs = ["proxy_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [":proxy"],
)

cc_library(
    name = "skeleton",
    srcs = ["skeleton.cpp"],
    hdrs = ["skeleton.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        "//score/mw/com/impl:binding_type",
        "//score/mw/com/impl:skeleton_binding",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_event",
    srcs = ["skeleton_event.cpp"],
    hdrs = ["skeleton_event.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:skeleton_event_binding",
    ],
)

cc_library(
    name = "skeleton_binding_factory",
    srcs = ["skeleton_binding_factory.cpp"],
    hdrs = ["skeleton_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [":skeleton"],
)

cc_library(
    name = "event_binding_factory",
    srcs = ["event_binding_factory.cpp"],
    hdrs = ["event_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        ":proxy_event",
        ":skeleton_event",
    ],
)
