# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_python//sphinxdocs:sphinx_docs_library.bzl", "sphinx_docs_library")
load("@score_tooling//api_surface:api_surface.bzl", "api_surface_test")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw/com/flags:flags.bzl", "if_e2e_enabled")

# Capture README.md for Sphinx documentation.
sphinx_docs_library(
    name = "readme_md",
    srcs = ["README.md"],
    prefix = "score/mw/com/",
    visibility = ["//docs/sphinx:__pkg__"],
)

cc_library(
    name = "com_error_domain",
    srcs = ["com_error_domain.cpp"],
    hdrs = ["com_error_domain.h"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = ["//score/mw/com/impl:error"],
)

cc_library(
    name = "types",
    srcs = ["types.cpp"],
    hdrs = ["types.h"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "//score/mw/com/flags:e2e_feature",
        "//score/mw/com/impl",
        "//score/mw/com/impl:event_receive_handler",
    ] + if_e2e_enabled(["//score/mw/com/impl/e2e:e2e_result"]),
)

cc_library(
    name = "runtime",
    srcs = ["runtime.cpp"],
    hdrs = ["runtime.h"],
    implementation_deps = ["@score_baselibs//score/language/futurecpp"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":runtime_configuration",
        ":types",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/mocking:i_runtime",
        "@score_baselibs//score/language/safecpp/string_view:zstring_view",
    ],
)

cc_library(
    name = "com",
    tags = ["FUSA"],
    visibility = ["//visibility:public"],
    deps = [
        ":com_error_domain",
        ":runtime",
        ":runtime_configuration",
        ":types",
    ],
)

cc_library(
    name = "runtime_configuration",
    srcs = ["runtime_configuration.cpp"],
    hdrs = ["runtime_configuration.h"],
    implementation_deps = [
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/string_manipulation/arguments",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "test_types",
    testonly = True,
    srcs = ["test_types.cpp"],
    hdrs = ["test_types.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":runtime",
        ":types",
        "//score/mw/com/impl",
        "//score/mw/com/impl/mocking:proxy_wrapper_class_test_view",
        "//score/mw/com/impl/mocking:skeleton_wrapper_class_test_view",
        "//score/mw/com/impl/mocking:test_type_utilities",
        "//score/mw/com/mocking:i_runtime",
        "//score/mw/com/mocking:test_type_utilities",
    ],
)

alias(
    name = "config_schema",
    actual = "//score/mw/com/impl/configuration:mw_com_config_schema",
    visibility = ["//visibility:public"],
)

alias(
    name = "config_fbs_schema",
    actual = "//score/mw/com/impl/configuration:mw_com_config.fbs",
    visibility = ["//visibility:public"],
)

cc_unit_test(
    name = "unit_test",
    srcs = [
        "runtime_test.cpp",
        "types_accessibility_test.cpp",
        "types_test.cpp",
    ],
    data = [
        "//score/mw/com/impl/configuration:mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":runtime",
        ":types",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/test:runtime_mock_guard",
        "@score_baselibs//score/mw/log",
    ],
)

cc_unit_test(
    name = "runtime_configuration_test",
    srcs = ["runtime_configuration_test.cpp"],
    deps = [
        "runtime_configuration",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/mw/log:recorder_mock",
    ],
)

# --- API Surface Stability Check ---

# The public API depends on the feature flags (see //score/mw/com/flags), e.g. E2E protection adds
# SamplePtr::GetE2EResult(). Its stability is only guaranteed in the default configuration, so the API surface is only
# checked there and the tests are skipped if E2E is enabled. The macro of api_surface_test has no
# target_compatible_with parameter, but targets depending on an incompatible target are incompatible as well.
cc_library(
    name = "com_for_api_surface",
    target_compatible_with = select({
        "//score/mw/com/flags:e2e_enabled": ["@platforms//:incompatible"],
        "//conditions:default": [],
    }),
    deps = [":com"],
)

api_surface_test(
    name = "api_surface_test",
    check_docs = False,
    lock_file = "api_surface.lock.json",
    target = ":com_for_api_surface",
)

api_surface_test(
    name = "api_surface_docs_test",
    check_docs = True,
    lock_file = "api_surface.lock.json",
    tags = ["manual"],
    target = ":com_for_api_surface",
)
