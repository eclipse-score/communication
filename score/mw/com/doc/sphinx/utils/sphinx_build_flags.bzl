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

"""
Feature flags for sphinx build configuration.

This module provides feature flags to switch between different sphinx build approaches
based on the target environment (Sphinx version, Python version, rules_python version).

Supported configurations:
- legacy_wrapper: For Sphinx <=7.2.*, Python 3.9, rules_python 1.2 (uses py_binary wrapper)
- native_binary: For Sphinx 9.*, Python 3.12, rules_python 1.5 (uses sphinx_build_binary)
"""

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

def sphinx_build_settings(name = "sphinx_build_flags", visibility = None):
    """
    Creates feature flags for selecting sphinx build implementation.

    Usage in BUILD files:
        load("//path/to/utils/utils:sphinx_build_flags.bzl", "sphinx_build_settings")
        sphinx_build_settings(visibility = ["//visibility:public"])

    Args:
        name: Name prefix for the generated targets (default: "sphinx_build_flags")
        visibility: Visibility list for the generated config_setting targets
    """

    # Feature flag to enable native sphinx_build_binary (for newer environments)
    bool_flag(
        name = "use_native_sphinx_build",
        build_setting_default = False,
        visibility = visibility,
    )

    # Config setting for legacy wrapper approach (default)
    native.config_setting(
        name = "use_wrapper",
        flag_values = {
            ":use_native_sphinx_build": "False",
        },
        visibility = visibility,
    )

    # Config setting for native sphinx_build_binary approach
    native.config_setting(
        name = "use_native",
        flag_values = {
            ":use_native_sphinx_build": "True",
        },
        visibility = visibility,
    )
