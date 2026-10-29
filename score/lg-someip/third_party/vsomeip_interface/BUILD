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

load("//score/lg-someip:lgsomeip.bzl", "lgsomeip_cc_binary", "lgsomeip_cc_library")

# Only the vsomeip compatibility headers are public. Core, OS abstraction,
# serializer, and E2E headers remain private implementation details.
lgsomeip_cc_library(
    name = "vsomeip_headers",
    hdrs = glob(["include/vsomeip/**/*"]),
    includes = ["include"],
)

# vsomeip compatible API implemented on top of lg-someip.
lgsomeip_cc_library(
    name = "vsomeip-if",
    srcs = glob([
        "source/*.cpp",
        "source/*.hpp",
    ]),
    implementation_deps = ["//score/lg-someip/src/someip:internal_headers"],
    deps = [
        ":vsomeip_headers",
        "//score/lg-someip/src/someip:lgsomeip",
    ],
)

[
    lgsomeip_cc_binary(
        name = sample,
        srcs = [
            "examples/" + sample + ".cpp",
            "examples/TimeCheck.h",
            "examples/sample-ids.hpp",
        ],
        deps = [":vsomeip-if"],
    )
    for sample in [
        "notify-sample",
        "request-sample",
        "response-sample",
        "subscribe-sample",
    ]
]

[
    lgsomeip_cc_binary(
        name = "hello_world_" + role,
        srcs = ["examples/hello_world/hello_world_%s.cpp" % role],
        deps = [":vsomeip-if"],
    )
    for role in [
        "service",
        "client",
    ]
]
