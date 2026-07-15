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

load("//quality/api_surface:api_surface.bzl", "api_surface_test", "api_surface_update")
load("@rules_python//python:defs.bzl", "py_test")

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

# API surface generation for each test case - these produce JSON files
# that the py_test will validate

api_surface_test(
    name = "basic_class_api_test",
    deps = [":basic_class_lib"],
    lock_file = "golden/basic_class.lock.json",
    check_docs = False,
    target_label = "//quality/api_surface/tests:basic_class_lib",
)

api_surface_test(
    name = "type_alias_api_test",
    deps = [":type_alias_lib"],
    lock_file = "golden/type_alias.lock.json",
    check_docs = False,
    target_label = "//quality/api_surface/tests:type_alias_lib",
)

api_surface_test(
    name = "inheritance_api_test",
    deps = [":inheritance_lib"],
    lock_file = "golden/inheritance.lock.json",
    check_docs = False,
    target_label = "//quality/api_surface/tests:inheritance_lib",
)

api_surface_test(
    name = "template_alias_api_test",
    deps = [":template_alias_lib"],
    lock_file = "golden/template_alias.lock.json",
    check_docs = False,
    target_label = "//quality/api_surface/tests:template_alias_lib",
)

api_surface_test(
    name = "private_changes_api_test",
    deps = [":private_changes_lib"],
    lock_file = "golden/private_changes.lock.json",
    check_docs = False,
    target_label = "//quality/api_surface/tests:private_changes_lib",
)

api_surface_test(
    name = "types_in_signatures_api_test",
    deps = [":types_in_signatures_lib"],
    lock_file = "golden/types_in_signatures.lock.json",
    check_docs = False,
    target_label = "//quality/api_surface/tests:types_in_signatures_lib",
)

api_surface_test(
    name = "enum_and_free_functions_api_test",
    deps = [":enum_and_free_functions_lib"],
    lock_file = "golden/enum_and_free_functions.lock.json",
    check_docs = False,
    target_label = "//quality/api_surface/tests:enum_and_free_functions_lib",
)

api_surface_test(
    name = "internal_namespace_api_test",
    deps = [":internal_namespace_lib"],
    lock_file = "golden/internal_namespace.lock.json",
    check_docs = False,
    target_label = "//quality/api_surface/tests:internal_namespace_lib",
)

# Update targets to regenerate golden files
api_surface_update(
    name = "basic_class_api_update",
    deps = [":basic_class_lib"],
    lock_file_path = "quality/api_surface/tests/golden/basic_class.lock.json",
    target_label = "//quality/api_surface/tests:basic_class_lib",
)

api_surface_update(
    name = "type_alias_api_update",
    deps = [":type_alias_lib"],
    lock_file_path = "quality/api_surface/tests/golden/type_alias.lock.json",
    target_label = "//quality/api_surface/tests:type_alias_lib",
)

api_surface_update(
    name = "inheritance_api_update",
    deps = [":inheritance_lib"],
    lock_file_path = "quality/api_surface/tests/golden/inheritance.lock.json",
    target_label = "//quality/api_surface/tests:inheritance_lib",
)

api_surface_update(
    name = "template_alias_api_update",
    deps = [":template_alias_lib"],
    lock_file_path = "quality/api_surface/tests/golden/template_alias.lock.json",
    target_label = "//quality/api_surface/tests:template_alias_lib",
)

api_surface_update(
    name = "private_changes_api_update",
    deps = [":private_changes_lib"],
    lock_file_path = "quality/api_surface/tests/golden/private_changes.lock.json",
    target_label = "//quality/api_surface/tests:private_changes_lib",
)

api_surface_update(
    name = "types_in_signatures_api_update",
    deps = [":types_in_signatures_lib"],
    lock_file_path = "quality/api_surface/tests/golden/types_in_signatures.lock.json",
    target_label = "//quality/api_surface/tests:types_in_signatures_lib",
)

api_surface_update(
    name = "enum_and_free_functions_api_update",
    deps = [":enum_and_free_functions_lib"],
    lock_file_path = "quality/api_surface/tests/golden/enum_and_free_functions.lock.json",
    target_label = "//quality/api_surface/tests:enum_and_free_functions_lib",
)

api_surface_update(
    name = "internal_namespace_api_update",
    deps = [":internal_namespace_lib"],
    lock_file_path = "quality/api_surface/tests/golden/internal_namespace.lock.json",
    target_label = "//quality/api_surface/tests:internal_namespace_lib",
)

# Python assertion test that validates the CONTENT of extracted API surfaces
# (not just that they match a lock file, but that specific expectations hold)
py_test(
    name = "api_extraction_validation_test",
    srcs = ["test_api_extraction_validation.py"],
    main = "test_api_extraction_validation.py",
    data = [
        ":basic_class_api_test_gen",
        ":type_alias_api_test_gen",
        ":inheritance_api_test_gen",
        ":template_alias_api_test_gen",
        ":private_changes_api_test_gen",
        ":types_in_signatures_api_test_gen",
        ":enum_and_free_functions_api_test_gen",
        ":internal_namespace_api_test_gen",
    ],
)
