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

load("//:score/common_features.bzl", "COMPILER_WARNING_FEATURES")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

cc_library(
    name = "message_error",
    srcs = ["message_error.cpp"],
    hdrs = ["message_error.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "serialization",
    hdrs = ["serialization.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        "//score/mw/com/gateway/transport_layer/sample/messages:message_error",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "gateway_messages",
    srcs = ["gateway_messages.cpp"],
    hdrs = ["gateway_messages.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        "//score/mw/com/gateway/transport_layer:transport",
        "//score/mw/com/gateway/transport_layer/sample/messages:serialization",
        "//score/mw/com/gateway/transport_layer/sample/messages:transport_message",
        "//score/mw/com/impl:instance_specifier",
    ],
)

cc_library(
    name = "transport_message",
    srcs = ["transport_message.cpp"],
    hdrs = ["transport_message.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_unit_test(
    name = "serialization_test",
    srcs = ["serialization_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/gateway/transport_layer:transport",
        "//score/mw/com/gateway/transport_layer/sample/messages:serialization",
    ],
)

cc_unit_test(
    name = "gateway_messages_test",
    srcs = ["gateway_messages_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/gateway/transport_layer/sample/messages:gateway_messages",
        "//score/mw/com/gateway/transport_layer/sample/messages:message_error",
        "//score/mw/com/impl:instance_specifier",
    ],
)

cc_unit_test(
    name = "message_error_test",
    srcs = ["message_error_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/gateway/transport_layer/sample/messages:message_error",
    ],
)
