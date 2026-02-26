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
load("@rules_python//sphinxdocs:sphinx.bzl", "sphinx_build_binary", "sphinx_docs")
load("@rules_python//sphinxdocs:sphinx_docs_library.bzl", "sphinx_docs_library")
load("//score/mw/com/doc/sphinx/utils:defs.bzl", "generate_api_rst")

# Create the sphinx-build binary with required dependencies
# This wrapper approach is used for older environments (e.g. Sphinx <=7.2.* and rules_python 1.2)
py_binary(
    name = "sphinx-build-wrapper",
    srcs = ["//score/mw/com/doc/sphinx/utils:sphinx_build_wrapper"],
    main = "sphinx_build_wrapper.py",
    deps = [
        "//third_party/breathe",
        "//third_party/myst_parser",
        "//third_party/pydata_sphinx_theme",
        "//third_party/sphinx",
        "//third_party/sphinx_rtd_theme",
    ],
)

# Native sphinx_build_binary for newer environments (e.g. Sphinx 9.*, rules_python 1.5)
sphinx_build_binary(
    name = "sphinx-build-native",
    deps = [
        "//third_party/breathe",
        "//third_party/myst_parser",
        "//third_party/pydata_sphinx_theme",
        "//third_party/sphinx",
        "//third_party/sphinx_rtd_theme",
    ],
)

# Alias to select  appropriate sphinx-build implementation based on feature flag
# Default: uses wrapper approach for compatibility with older environment
# Use --//path/to/sphinx/utils:use_native_sphinx_build for native binary
alias(
    name = "sphinx-build",
    actual = select({
        "//score/mw/com/doc/sphinx/utils:use_native": ":sphinx-build-native",
        "//score/mw/com/doc/sphinx/utils:use_wrapper": ":sphinx-build-wrapper",
    }),
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
