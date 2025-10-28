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
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "configuration_test_resources",
    testonly = True,
    srcs = ["configuration_test_resources.cpp"],
    hdrs = ["configuration_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "//score/mw/com/impl/configuration",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "configuration_store",
    testonly = True,
    srcs = ["configuration_store.cpp"],
    hdrs = ["configuration_store.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "//score/mw/com/impl:enriched_instance_identifier",
        "//score/mw/com/impl:handle_type",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl/configuration",
    ],
)

validate_json_schema_test(
    name = "invalid_json_wrong_consumer_name",
    expected_failure = True,
    json = "invalid_json_consumer_name.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "invalid_json_wrong_provider_name",
    expected_failure = True,
    json = "invalid_json_provider_name.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "invalid_json_unknown_permission_check_value",
    expected_failure = True,
    json = "invalid_json_unknown_permission_check_value.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "invalid_json_incomplete_service_type",
    expected_failure = True,
    json = "invalid_json_incomplete_service_type.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "invalid_json_incomplete_binding",
    expected_failure = True,
    json = "invalid_json_incomplete_binding.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "invalid_json_incomplete_event",
    expected_failure = True,
    json = "invalid_json_incomplete_event.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "invalid_json_incomplete_field",
    expected_failure = True,
    json = "invalid_json_incomplete_field.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "invalid_json_incomplete_service_instance",
    expected_failure = True,
    json = "invalid_json_incomplete_service_instance.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "invalid_json_incomplete_instance",
    expected_failure = True,
    json = "invalid_json_incomplete_instance.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "invalid_json_incomplete_instance_event",
    expected_failure = True,
    json = "invalid_json_incomplete_instance_event.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "invalid_json_incomplete_instance_field",
    expected_failure = True,
    json = "invalid_json_incomplete_instance_field.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)
