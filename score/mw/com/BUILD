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
load("@score-baselibs//bazel:unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")
load("@score-baselibs//score/quality/clang_tidy:extra_checks.bzl", "clang_tidy_extra_checks")

#Seooc(
#    feat_reqs =
#    sw_arch_design =
#)

# sw_comp(
#   comp_reqs = "doc/comp_req_lola"
#   comp_arch = "*.puml"
#   src = "bazel_target_name"
#   comp_int_test = "bazel target name"
#)


cc_library(
    name = "com_error_domain",
    srcs = ["com_error_domain.cpp"],
    hdrs = ["com_error_domain.h"],
    features = COMPILER_WARNING_FEATURES,
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
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl",
        "//score/mw/com/impl:event_receive_handler",
    ],
)

cc_library(
    name = "runtime",
    srcs = ["runtime.cpp"],
    hdrs = ["runtime.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score-baselibs//score/language/futurecpp"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":types",
        "//score/mw/com/impl:runtime",
    ],
)

cc_library(
    name = "com",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FUSA"],
    visibility = ["//visibility:public"],
    deps = [
        ":com_error_domain",
        ":runtime",
        ":types",
    ],
)

alias(
    name = "config_schema",
    actual = "//score/mw/com/impl/configuration:ara_com_config_schema",
    visibility = ["//visibility:public"],
)

cc_gtest_unit_test(
    name = "unit_test",
    srcs = [
        #"runtime_test.cpp",
        #"types_accessibility_test.cpp",
        "types_test.cpp",
    ],
    data = [
        "//score/mw/com/impl/configuration:ara_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com:__pkg__",
    ],
    deps = [
        ":runtime",
        ":types",
        "//score/mw/com/impl:runtime_mock",
        "@score-baselibs//score/mw/log",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":unit_test",
    ],
    test_suites_from_sub_packages = [
        "//score/mw/com/impl:unit_test_suite",
        "//score/mw/com/message_passing:unit_test_suite",
        # "//score/mw/com/test/api:unit_test_suite", Remove until API tests are open sourced Ticket-196988
    ],
    visibility = [
        "//platform/aas/mw:__pkg__",
    ],
)

clang_tidy_extra_checks(
    name = "clang_tidy_extra_checks",
    tidy_config_file = ".clang-tidy-extra",
)
