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

# --- Additional API surface cases (multi-inheritance, ref-qualifiers, member
# function templates, globals, protected methods, constexpr, forward/incomplete
# types, extern "C", extern templates, C++ attributes, SFINAE) ---

# Minimal cc_library targets for the additional test headers.

cc_library(
    name = "multi_inheritance_lib",
    hdrs = ["multi_inheritance.h"],
)

cc_library(
    name = "ref_qualifiers_lib",
    hdrs = ["ref_qualifiers.h"],
)

cc_library(
    name = "member_function_templates_lib",
    hdrs = ["member_function_templates.h"],
)

cc_library(
    name = "globals_lib",
    hdrs = ["globals.h"],
)

cc_library(
    name = "protected_methods_lib",
    hdrs = ["protected_methods.h"],
)

cc_library(
    name = "constexpr_members_lib",
    hdrs = ["constexpr_members.h"],
)

cc_library(
    name = "forward_declared_lib",
    hdrs = ["forward_declared.h"],
)

cc_library(
    name = "extern_c_lib",
    hdrs = ["extern_c.h"],
)

cc_library(
    name = "extern_explicit_lib",
    hdrs = ["extern_explicit.h"],
)

cc_library(
    name = "cpp_attributes_lib",
    hdrs = ["cpp_attributes.h"],
)

cc_library(
    name = "sfinae_lib",
    hdrs = ["sfinae.h"],
)

# API surface generation + comparison for the additional cases.

api_surface_test(
    name = "multi_inheritance_api_test",
    check_docs = False,
    lock_file = "golden/multi_inheritance.lock.json",
    target = ":multi_inheritance_lib",
)

api_surface_test(
    name = "ref_qualifiers_api_test",
    check_docs = False,
    lock_file = "golden/ref_qualifiers.lock.json",
    target = ":ref_qualifiers_lib",
)

api_surface_test(
    name = "member_function_templates_api_test",
    check_docs = False,
    lock_file = "golden/member_function_templates.lock.json",
    target = ":member_function_templates_lib",
)

api_surface_test(
    name = "globals_api_test",
    check_docs = False,
    lock_file = "golden/globals.lock.json",
    target = ":globals_lib",
)

api_surface_test(
    name = "protected_methods_api_test",
    check_docs = False,
    lock_file = "golden/protected_methods.lock.json",
    target = ":protected_methods_lib",
)

api_surface_test(
    name = "constexpr_members_api_test",
    check_docs = False,
    lock_file = "golden/constexpr_members.lock.json",
    target = ":constexpr_members_lib",
)

api_surface_test(
    name = "forward_declared_api_test",
    check_docs = False,
    lock_file = "golden/forward_declared.lock.json",
    target = ":forward_declared_lib",
)

api_surface_test(
    name = "extern_c_api_test",
    check_docs = False,
    lock_file = "golden/extern_c.lock.json",
    target = ":extern_c_lib",
)

api_surface_test(
    name = "extern_explicit_api_test",
    check_docs = False,
    lock_file = "golden/extern_explicit.lock.json",
    target = ":extern_explicit_lib",
)

api_surface_test(
    name = "cpp_attributes_api_test",
    check_docs = False,
    lock_file = "golden/cpp_attributes.lock.json",
    target = ":cpp_attributes_lib",
)

api_surface_test(
    name = "sfinae_api_test",
    check_docs = False,
    lock_file = "golden/sfinae.lock.json",
    target = ":sfinae_lib",
)

# Python assertion test that validates the CONTENT of extracted API surfaces
# (not just that they match a lock file, but that specific expectations hold)
py_test(
    name = "api_extraction_validation_test",
    srcs = ["test_api_extraction_validation.py"],
    data = [
        ":basic_class_api_test_gen",
        ":constexpr_members_api_test_gen",
        ":cpp_attributes_api_test_gen",
        ":enum_and_free_functions_api_test_gen",
        ":extern_c_api_test_gen",
        ":extern_explicit_api_test_gen",
        ":forward_declared_api_test_gen",
        ":globals_api_test_gen",
        ":inheritance_api_test_gen",
        ":internal_namespace_api_test_gen",
        ":member_function_templates_api_test_gen",
        ":multi_inheritance_api_test_gen",
        ":private_changes_api_test_gen",
        ":protected_methods_api_test_gen",
        ":ref_qualifiers_api_test_gen",
        ":result_type_regression_api_test_gen",
        ":sfinae_api_test_gen",
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
