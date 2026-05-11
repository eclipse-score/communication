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

load("@rules_python//sphinxdocs:sphinx_docs_library.bzl", "sphinx_docs_library")
load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "filter_execpath",
    "sphinx_module",
)
load("//docs/sphinx/utils:defs.bzl", "generate_api_rst")

# Capture the Doxygen XML output for Breathe
sphinx_docs_library(
    name = "doxygen_xml",
    srcs = ["//score/mw/com/design/doxygen_build:generate_doxygen"],
)

# Static assets for Sphinx documentation
filegroup(
    name = "static_assets",
    srcs = glob(["_static/**/*"]),
)

# Generate RST files from @api tagged items using custom Starlark rule
generate_api_rst(
    name = "generate_api_rst",
    doxygen_xml = "//score/mw/com/design/doxygen_build:generate_doxygen",
    output_dir = "generated",
    project_name = "mw::com",
)

filter_execpath(
    name = "breathe_doxygen_xml",
    filter_pattern = "doxygen_build/xml",
    flag = "-Dbreathe_projects.doxygen_build",
    target = "//docs/sphinx:doxygen_xml",
)

# Build Sphinx documentation
sphinx_module(
    name = "sphinx_doc",
    srcs =
        [
            "how_to_document.rst",
            "index.rst",
            "introduction.rst",
            "message_passing.rst",
            "safety_reports.rst",
            ":doxygen_xml",
            ":generate_api_rst",
            ":static_assets",
        ],
    docs_library_deps = [
        "//score/mw/com:readme_md",
    ],
    exec_compatible_with = ["@platforms//os:linux"],
    extra_opts = [
        "-Dbreathe_default_project=doxygen_build",
    ],
    extra_opts_targets = [
        # Pass location of Doxygen XML output to Breathe via -D option
        ":breathe_doxygen_xml",
    ],
    index = "index.rst",
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
    deps = [
    ],
)
