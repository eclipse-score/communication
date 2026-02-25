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

load("@rules_python//python:defs.bzl", "py_binary", "py_library")
load(":sphinx_build_flags.bzl", "sphinx_build_settings")

# Setup feature flags for sphinx build selection
sphinx_build_settings(visibility = ["//score/mw/com/doc/sphinx:__subpackages__"])

py_library(
    name = "sphinx_build_wrapper",
    srcs = ["sphinx_build_wrapper.py"],
    visibility = ["//score/mw/com/doc/sphinx:__pkg__"],
)

py_binary(
    name = "generate_api_rst_bin",
    srcs = ["extract_api_items.py"],
    main = "extract_api_items.py",
    visibility = ["//score/mw/com/doc/sphinx:__pkg__"],
)
