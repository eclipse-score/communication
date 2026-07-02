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

load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "unit_design")

# Class diagrams are passed through directly to Sphinx (sphinxcontrib-plantuml
# renders them at doc-build time). The RST wrapper references the .puml file
# via '.. uml::' so both must be listed together in 'static'.
unit_design(
    name = "client_connection_unit_design",
    static = [
        "client_connection_class_diagram.puml",
        "client_connection_design.rst",
    ],
    visibility = ["//score/message_passing:__pkg__"],
)

unit_design(
    name = "unix_domain_unit_design",
    static = [
        "unix_domain_class_diagram.puml",
        "unix_domain_design.rst",
    ],
    visibility = ["//score/message_passing:__pkg__"],
)

unit_design(
    name = "qnx_dispatch_unit_design",
    static = [
        "qnx_dispatch_class_diagram.puml",
        "qnx_dispatch_design.rst",
    ],
    visibility = ["//score/message_passing:__pkg__"],
)
