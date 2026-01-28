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

load("@rules_python//python:defs.bzl", "py_binary")
load("@rules_python//sphinxdocs:sphinx.bzl", "sphinx_docs")
load("@rules_python//sphinxdocs:sphinx_docs_library.bzl", "sphinx_docs_library")
load("//score/mw/com/doc/sphinx/utils:defs.bzl", "generate_api_rst")

# Create the sphinx-build binary with required dependencies
py_binary(
    name = "sphinx-build",
    srcs = ["//score/mw/com/doc/sphinx/utils:sphinx_build_wrapper"],
    main = "sphinx_build_wrapper.py",
    deps = [
        "@score_communication_pip//breathe",
        "@score_communication_pip//myst_parser",
        "@score_communication_pip//pydata_sphinx_theme",
        "@score_communication_pip//sphinx",
        "@score_communication_pip//sphinx_rtd_theme",
    ],
)

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

# Build Sphinx documentation
sphinx_docs(
    name = "docs",
    srcs = [
        "how_to_document.rst",
        "index.rst",
        "introduction.rst",
        "message_passing.rst",
        ":generate_api_rst",
        ":static_assets",
    ],
    config = "conf.py",
    formats = ["html"],
    sphinx = ":sphinx-build",
    deps = [
        ":doxygen_xml",
        "//score/mw/com:readme_md",
    ],
)
