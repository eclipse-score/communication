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

load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
load("//score/mw/com/impl/configuration/converter:json_to_flatbuffer_rule.bzl", "json_to_flatbuffer")

# Configurations used by configuration_parsing_strategy_test. Each <name>.json is parsed by
# ConfigurationJsonParsingStrategy; its <name>.bin (converted from the same JSON) is parsed by
# ConfigurationFlatbufferParsingStrategy.
SHARED_CONFIGS = [
    "application_id_uint32_max",
    "duplicate_instance_event_name",
    "duplicate_instance_field_name",
    "duplicate_instance_method_name",
    "duplicate_service_instance",
    "duplicate_service_type",
    "duplicate_type_event_id",
    "duplicate_type_event_name",
    "duplicate_type_field_id",
    "duplicate_type_field_name",
    "duplicate_type_method_name",
    "empty_deployment_instances",
    "empty_instance_specifier",
    "empty_service_elements",
    "empty_service_types_and_instances",
    "event_deprecated_max_samples",
    "event_enforce_max_samples_false",
    "event_max_samples_and_number_of_sample_slots",
    "field_enforce_max_samples_false",
    "field_use_flags_both_false",
    "field_use_flags_omitted",
    "field_use_get_if_available_false",
    "field_use_set_if_available_false",
    "instance_event_without_name",
    "instance_field_without_name",
    "instance_method_without_name",
    "inter_vm_forwarded_without_inter_vm_support",
    "inter_vm_support_without_inter_vm_forwarded",
    "invalid_instance_specifier",
    "malformed_asil_level_not_a_string",
    "malformed_permission_checks_not_a_string",
    "malformed_shm_size_calc_mode_not_a_string",
    "matching_event_and_field_ids",
    "maximum_nesting",
    "method_all_attributes",
    "method_defaults",
    "method_id_matches_event_id",
    "missing_service_instance_minor_version",
    "missing_service_type_major_version",
    "missing_service_type_version",
    "multiple_deployments_per_instance",
    "multiple_events_and_fields",
    "multiple_service_instances",
    "no_deployment_instances",
    "no_event_max_subscribers",
    "no_field_max_subscribers",
    "no_instance_asil_level",
    "no_instance_binding",
    "no_instance_major_version",
    "no_instance_specifier",
    "no_instance_specifier_in_empty_instance",
    "no_instance_version",
    "no_inter_vm_support",
    "no_service_element_lists",
    "no_service_instances",
    "no_service_type_binding_identifier",
    "no_service_type_bindings",
    "no_service_type_name",
    "no_service_type_service_id",
    "no_service_type_version",
    "no_service_types",
    "no_shm_instance_id",
    "no_type_event_id",
    "no_type_event_name",
    "no_type_field_id",
    "no_type_field_name",
    "no_type_method_id",
    "no_type_method_name",
    "permission_checks_strict",
    "process_asil_b",
    "process_asil_qm",
    "queue_size_only_b_receiver",
    "queue_size_only_b_sender",
    "queue_size_only_qm_receiver",
    "service_types_with_empty_service_instances",
    "service_types_without_service_instances",
    "shm_size_calc_mode_analysis",
    "shm_size_calc_mode_simulation",
    "single_instance_event",
    "single_instance_field",
    "someip_binding",
    "tracing_disabled",
    "tracing_enabled",
    "tracing_enabled_with_application_instance_id",
    "tracing_enabled_without_application_instance_id",
    "tracing_only_application_instance_id",
    "tracing_service_elements_enabled",
    "tracing_service_elements_global_disabled",
    "tracing_service_elements_partially_enabled",
    "tracing_slots_255",
    "tracing_without_filter_config_path",
    "valid_instance_specifier",
]

# Configurations which flatc refuses to convert (unknown keys, wrong JSON types, unknown enum symbols,
# out-of-range numbers), so they cannot be built with json_to_flatbuffer. They are only parsed by
# ConfigurationJsonParsingStrategy. For the FlatBuffer path, the test is that the JSON schema generated from
# mw_com_config.fbs rejects them.
FLATC_REJECTED_CONFIGS = [
    "allowed_consumer_invalid_quality_type",
    "application_id_negative",
    "application_id_too_large",
    "event_max_concurrent_allocations",
    "field_max_concurrent_allocations",
    "instance_event_unknown_name_key",
    "instance_field_unknown_name_key",
    "malformed_allowed_consumer_not_an_object",
    "malformed_deployment_instance_not_an_object",
    "malformed_instance_specifier_not_a_string",
    "malformed_service_instances_not_a_list",
    "malformed_service_type_name_not_a_string",
    "malformed_version_not_an_object",
    "process_asil_any",
    "process_asil_elefant",
    "process_asil_empty_string",
    "queue_size_b_receiver_not_a_number",
    "queue_size_b_receiver_not_a_number_with_b_sender",
    "queue_size_b_sender_not_a_number",
    "queue_size_qm_receiver_not_a_number",
    "shm_size_calc_mode_unknown",
    "tracing_slots_256",
    "unknown_instance_binding",
    "unknown_service_type_binding",
    "wrong_permission_checks_value",
]

[
    json_to_flatbuffer(
        name = config,
        json = config + ".json",
    )
    for config in SHARED_CONFIGS
]

# Why the JSON schema and not flatc itself: flatc is only used via the baselibs rules (serialize_buffer, wrapped by
# json_to_flatbuffer), which are build rules. A config rejected by flatc therefore fails the build action, and Bazel
# cannot turn a failing action into a passing test (rules_build_error only supports C++ targets). Asserting the
# rejection with flatc would mean invoking flatc directly from a custom test rule, bypassing baselibs.
# TODO: Replace these schema tests with build failure tests of json_to_flatbuffer once rules_build_error supports
# non-C++ targets (see https://github.com/yuyawk/rules_build_error/issues/218)
# mw_com_config_schema.json is generated from mw_com_config.fbs (kept in sync by schema_drift_test), so it rejects
# every config listed here as well. Note that the schema is stricter than flatc (e.g. required keys, min/max), so it
# also rejects some SHARED_CONFIGS: when adding a config here, make sure json_to_flatbuffer really fails to build it.
[
    validate_json_schema_test(
        name = config + "_schema_rejects",
        expected_failure = True,
        json = config + ".json",
        schema = "//score/mw/com/impl/configuration:mw_com_config_schema",
        tags = ["lint"],
    )
    for config in FLATC_REJECTED_CONFIGS
]

filegroup(
    name = "test_configs_json",
    srcs = [config + ".json" for config in SHARED_CONFIGS + FLATC_REJECTED_CONFIGS],
    visibility = ["//score/mw/com/impl/configuration:__subpackages__"],
)

filegroup(
    name = "test_configs_bin",
    srcs = [":" + config for config in SHARED_CONFIGS],
    visibility = ["//score/mw/com/impl/configuration:__subpackages__"],
)
