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

load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "component",
    "component_requirements",
    "unit",
)

component_requirements(
    name = "component_requirements",
    srcs = ["//score/message_passing/dependability/requirements:component_requirements_trlc"],
    deps = ["//score/message_passing/dependability:feature_requirements"],
    visibility = ["//visibility:public"],
)

component(
    name = "component_message_passing",
    components = [
        ":client",
        ":server",
    ],
    requirements = [
        ":component_requirements",
    ],
    tests = [],
    visibility = ["//visibility:public"],
)

# Component: client
# Covers the client-side connection management and common messaging infrastructure
component(
    name = "client",
    components = [
        ":client_connection",
    ],
    requirements = [
        ":component_requirements",
    ],
    tests = [],
    visibility = ["//visibility:public"],
)

unit(
    name = "client_connection",
    unit_design = ["//score/message_passing/design:client_connection_unit_design"],
    implementation = [
        "//score/message_passing:message_passing_common",
    ],
    scope = [
        "//score/message_passing:message_passing_common",
    ],
    tests = [
        "//score/message_passing:client_connection_test",
    ],
    visibility = ["//visibility:public"],
)

# Component: server
# Covers the platform-specific server implementations (QNX dispatch and Unix domain)
component(
    name = "server",
    components = select({
        "@platforms//os:qnx": [":qnx_dispatch", ":unix_domain"],
        "//conditions:default": [":unix_domain"],
    }),
    requirements = [
        ":component_requirements",
    ],
    tests = [],
    visibility = ["//visibility:public"],
)

unit(
    name = "qnx_dispatch",
    unit_design = ["//score/message_passing/design:qnx_dispatch_unit_design"],
    implementation = [
        "//score/message_passing:message_passing_qnx_dispatch",
    ],
    scope = [
        "//score/message_passing:message_passing_qnx_dispatch",
    ],
    tests = [
        "//score/message_passing:qnx_dispatch_test",
    ],
    visibility = ["//visibility:public"],
)

unit(
    name = "unix_domain",
    unit_design = ["//score/message_passing/design:unix_domain_unit_design"],
    implementation = [
        "//score/message_passing:message_passing_unix_domain",
    ],
    scope = [
        "//score/message_passing:message_passing_unix_domain",
    ],
    tests = [
        "//score/message_passing:unix_domain_test",
    ],
    visibility = ["//visibility:public"],
)
