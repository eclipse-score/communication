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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")

validate_json_schema_test(
    name = "validate_config_schema",
    json = "config/mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

cc_library(
    name = "test_event_datatype",
    testonly = True,
    hdrs = ["test_event_datatype.h"],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "test_parameters",
    testonly = True,
    hdrs = ["test_parameters.h"],
)

# Verifies that none of the file descriptors opened by the mw::com runtime is inherited by a child process created with
# fork() + execve() (i.e. every one of them has FD_CLOEXEC set). Each scenario is its own test.
# See https://github.com/eclipse-score/communication/issues/1064.
[
    cc_test(
        name = "close_on_exec_{}_test".format(scenario),
        srcs = ["close_on_exec_test.cpp"],
        args = [
            "$(location config/mw_com_config.json)",
            scenario,
        ],
        data = ["config/mw_com_config.json"],
        features = [
            "aborts_upon_exception",
        ],
        tags = [
            # All scenarios offer the same service instance, so they must not run at the same time.
            "exclusive",
            "integration-test",
        ],
        # Relies on /proc/self/{fd,exe} and popen(), which are not available on QNX.
        target_compatible_with = ["@platforms//os:linux"],
        deps = [
            ":test_event_datatype",
            ":test_parameters",
            "//score/mw/com",
        ],
    )
    for scenario in [
        "service_offered",
        "proxy_subscribed",
        "service_offered_again",
    ]
]
