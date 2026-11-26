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
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

cc_library(
    name = "sd_client",
    srcs = ["sd_client.cpp"],
    hdrs = [
        "sd_client.h",
        "sd_protocol.h",
    ],
    visibility = [
        "//score/mw/com/impl/bindings/someip:__pkg__",
        "//score/mw/com/impl/bindings/someip/test:__pkg__",
    ],
    deps = [
        "//score/message_passing:common_headers",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_unit_test(
    name = "sd_client_test",
    srcs = ["sd_client_test.cpp"],
    deps = [
        ":sd_client",
        "//score/message_passing:common_headers",
        "//score/message_passing:mock",
        "@score_baselibs//score/language/futurecpp",
    ],
)
