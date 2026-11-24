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

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_python//python:defs.bzl", "py_test")

# Gates the (experimental) FlatBuffers configuration path. When set, the runtime
# parses configuration files ending in .bfbs/.bin with the FlatBuffers config
# parsing strategy (all others as JSON), and the generated flatbuffer_cc_library
# and related targets are compiled. When unset, configurations are always parsed
# as JSON. Enable with --config=flatbuffers (see .bazelrc).
# Experimental feature flag: must be part of --config=activate_all_experimental_features
# in .bazelrc (see CI.md#feature-flags).
bool_flag(
    name = "experimental_enable_flatbuffers_configuration",
    build_setting_default = False,
    visibility = ["//score/mw/com:__subpackages__"],
)

config_setting(
    name = "flatbuffers_enabled",
    flag_values = {
        ":experimental_enable_flatbuffers_configuration": "True",
    },
    visibility = ["//score/mw/com:__subpackages__"],
)

# Propagates SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED to every target which (transitively) depends on it.
# Targets that contain `#if defined(SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED)` must depend on this library.
cc_library(
    name = "flatbuffers_feature",
    defines = select({
        ":flatbuffers_enabled": ["SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED"],
        "//conditions:default": [],
    }),
    visibility = ["//score/mw/com:__subpackages__"],
)

# Gates the (experimental) SOME/IP binding: its configuration types, the binding implementation and the
# SOME/IP specific code paths in the plumbing/runtime. Enable with --config=someip (see .bazelrc).
# Experimental feature flag: must be part of --config=activate_all_experimental_features
# in .bazelrc (see CI.md#feature-flags).
bool_flag(
    name = "experimental_enable_someip_binding",
    build_setting_default = False,
    visibility = ["//score/mw/com:__subpackages__"],
)

config_setting(
    name = "someip_enabled",
    flag_values = {
        ":experimental_enable_someip_binding": "True",
    },
    visibility = ["//score/mw/com:__subpackages__"],
)

# Propagates SCORE_MW_COM_SOMEIP_BINDING_ENABLED to every target which (transitively) depends on it. Targets that
# contain `#if defined(SCORE_MW_COM_SOMEIP_BINDING_ENABLED)` must depend on this library, so that all translation units
# agree on the layout of types such as the binding variants.
cc_library(
    name = "someip_feature",
    defines = select({
        ":someip_enabled": ["SCORE_MW_COM_SOMEIP_BINDING_ENABLED"],
        "//conditions:default": [],
    }),
    visibility = ["//score/mw/com:__subpackages__"],
)

# Gates the (experimental) E2E protection: the E2E result carried by SamplePtr, its public API, the E2E event deployment
# configuration and the E2E building blocks in //score/mw/com/impl/e2e. Enable with --config=e2e (see .bazelrc).
# Experimental feature flag: must be part of --config=activate_all_experimental_features
# in .bazelrc (see CI.md#feature-flags).
bool_flag(
    name = "experimental_enable_e2e_protection",
    build_setting_default = False,
    visibility = ["//score/mw/com:__subpackages__"],
)

config_setting(
    name = "e2e_enabled",
    flag_values = {
        ":experimental_enable_e2e_protection": "True",
    },
    visibility = ["//score/mw/com:__subpackages__"],
)

# Propagates SCORE_MW_COM_E2E_PROTECTION_ENABLED to every target which (transitively) depends on it. Targets that
# contain `#if defined(SCORE_MW_COM_E2E_PROTECTION_ENABLED)` must depend on this library, so that all translation units
# agree on the layout of types such as SamplePtr.
cc_library(
    name = "e2e_feature",
    defines = select({
        ":e2e_enabled": ["SCORE_MW_COM_E2E_PROTECTION_ENABLED"],
        "//conditions:default": [],
    }),
    visibility = ["//score/mw/com:__subpackages__"],
)

# Checks the rules for feature flags of CI.md#feature-flags: every experimental_enable_* flag of this package defaults to
# False, has a config in .bazelrc and is activated by --config=activate_all_experimental_features.
py_test(
    name = "experimental_flags_test",
    srcs = ["experimental_flags_test.py"],
    data = [
        "BUILD",
        "//:.bazelrc",
    ],
    env = {
        "BAZELRC_FILE": "$(rootpath //:.bazelrc)",
        "FLAGS_BUILD_FILE": "$(rootpath :BUILD)",
    },
    target_compatible_with = ["@platforms//os:linux"],
)
