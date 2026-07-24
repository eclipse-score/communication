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

load("@rules_python//python:defs.bzl", "py_test")
load("//quality/api_surface:api_surface.bzl", "api_surface_test")

# Minimal cc_library targets for test headers (provides CcInfo for includes)

cc_library(
    name = "basic_class_lib",
    hdrs = ["basic_class.h"],
)

cc_library(
    name = "type_alias_lib",
    hdrs = ["type_alias.h"],
)

cc_library(
    name = "inheritance_lib",
    hdrs = ["inheritance.h"],
)

cc_library(
    name = "template_alias_lib",
    hdrs = ["template_alias.h"],
)

cc_library(
    name = "private_changes_lib",
    hdrs = ["private_changes.h"],
)

cc_library(
    name = "types_in_signatures_lib",
    hdrs = ["types_in_signatures.h"],
)

cc_library(
    name = "enum_and_free_functions_lib",
    hdrs = ["enum_and_free_functions.h"],
)

cc_library(
    name = "internal_namespace_lib",
    hdrs = ["internal_namespace.h"],
)

cc_library(
    name = "result_type_support_lib",
    hdrs = ["result_type_support.h"],
)

cc_library(
    name = "result_type_regression_lib",
    hdrs = ["result_type_regression.h"],
    deps = [":result_type_support_lib"],
)

# API surface generation for each test case - these produce JSON files
# that the py_test will validate

api_surface_test(
    name = "basic_class_api_test",
    check_docs = False,
    lock_file = "golden/basic_class.lock.json",
    target = ":basic_class_lib",
)

api_surface_test(
    name = "type_alias_api_test",
    check_docs = False,
    lock_file = "golden/type_alias.lock.json",
    target = ":type_alias_lib",
)

api_surface_test(
    name = "inheritance_api_test",
    check_docs = False,
    lock_file = "golden/inheritance.lock.json",
    target = ":inheritance_lib",
)

api_surface_test(
    name = "template_alias_api_test",
    check_docs = False,
    lock_file = "golden/template_alias.lock.json",
    target = ":template_alias_lib",
)

api_surface_test(
    name = "private_changes_api_test",
    check_docs = False,
    lock_file = "golden/private_changes.lock.json",
    target = ":private_changes_lib",
)

api_surface_test(
    name = "types_in_signatures_api_test",
    check_docs = False,
    lock_file = "golden/types_in_signatures.lock.json",
    target = ":types_in_signatures_lib",
)

api_surface_test(
    name = "enum_and_free_functions_api_test",
    check_docs = False,
    lock_file = "golden/enum_and_free_functions.lock.json",
    target = ":enum_and_free_functions_lib",
)

api_surface_test(
    name = "internal_namespace_api_test",
    check_docs = False,
    lock_file = "golden/internal_namespace.lock.json",
    target = ":internal_namespace_lib",
)

api_surface_test(
    name = "result_type_regression_api_test",
    check_docs = False,
    lock_file = "golden/result_type_regression.lock.json",
    target = ":result_type_regression_lib",
)

# Python assertion test that validates the CONTENT of extracted API surfaces
# (not just that they match a lock file, but that specific expectations hold)
py_test(
    name = "api_extraction_validation_test",
    srcs = ["test_api_extraction_validation.py"],
    data = [
        ":basic_class_api_test_gen",
        ":enum_and_free_functions_api_test_gen",
        ":inheritance_api_test_gen",
        ":internal_namespace_api_test_gen",
        ":private_changes_api_test_gen",
        ":result_type_regression_api_test_gen",
        ":template_alias_api_test_gen",
        ":type_alias_api_test_gen",
        ":types_in_signatures_api_test_gen",
    ],
    main = "test_api_extraction_validation.py",
)

py_test(
    name = "test_diff_api",
    srcs = ["test_diff_api.py"],
    main = "test_diff_api.py",
    deps = ["//quality/api_surface:diff_api_lib"],
)
