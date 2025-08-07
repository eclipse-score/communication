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
load("@score-baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

package(default_visibility = [
    "//score/mw/com/impl:__subpackages__",
])

cc_library(
    name = "impl",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":generic_proxy",
        ":generic_proxy_event",
        ":proxy_event",
        ":proxy_field",
        ":skeleton_event",
        ":skeleton_field",
        ":traits",
    ],
)

cc_library(
    name = "generic_proxy",
    srcs = ["generic_proxy.cpp"],
    hdrs = ["generic_proxy.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        ":error",
        "//score/mw/com/impl/plumbing",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":generic_proxy_event",
        ":handle_type",
        ":proxy_base",
        ":proxy_binding",
        ":service_element_map",
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "generic_proxy_event",
    srcs = ["generic_proxy_event.cpp"],
    hdrs = ["generic_proxy_event.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl/plumbing:event",
        "@score-baselibs//score/language/futurecpp",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":generic_proxy_event_binding",
        ":proxy_base",
        ":proxy_event_base",
        "//score/mw/com/impl/tracing:proxy_event_tracing",
        "@score-baselibs//score/mw/log",
    ],
)

cc_library(
    name = "proxy_event",
    srcs = ["proxy_event.cpp"],
    hdrs = ["proxy_event.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":generic_proxy_event",
        ":instance_identifier",
        ":proxy_base",
        ":proxy_event_base",
        ":proxy_event_binding",
        ":runtime",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/plumbing:event",
        "//score/mw/com/impl/tracing:proxy_event_tracing",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_event",
    srcs = ["skeleton_event.cpp"],
    hdrs = ["skeleton_event.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":instance_identifier",
        ":runtime",
        ":skeleton_base",
        ":skeleton_event_base",
        ":skeleton_event_binding",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/plumbing:event",
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "//score/mw/com/impl/tracing:skeleton_event_tracing",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_field",
    srcs = ["skeleton_field.cpp"],
    hdrs = ["skeleton_field.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":skeleton_event",
        ":skeleton_field_base",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/plumbing:field",
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "traits",
    srcs = ["traits.cpp"],
    hdrs = ["traits.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":proxy_base",
        ":proxy_event",
        ":proxy_field",
        ":skeleton_base",
        ":skeleton_event",
        ":skeleton_field",
        "//score/mw/com/impl/plumbing:event",
    ],
)

cc_library(
    name = "proxy_field",
    srcs = ["proxy_field.cpp"],
    hdrs = ["proxy_field.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":proxy_event",
        ":proxy_event_binding",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/plumbing:field",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_event_base",
    srcs = ["skeleton_event_base.cpp"],
    hdrs = ["skeleton_event_base.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":flag_owner",
        ":skeleton_event_binding",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/tracing:skeleton_event_tracing",
        "//score/mw/com/impl/tracing:skeleton_event_tracing_data",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_field_base",
    srcs = ["skeleton_field_base.cpp"],
    hdrs = ["skeleton_field_base.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":error",
        ":skeleton_event_base",
        "@score-baselibs//score/result",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_base",
    srcs = ["skeleton_base.cpp"],
    hdrs = ["skeleton_base.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__pkg__",
    ],
    deps = [
        ":flag_owner",
        ":instance_identifier",
        ":runtime",
        ":skeleton_binding",
        ":skeleton_event_base",
        ":skeleton_field_base",
        "//score/mw/com/impl/plumbing",
        "//score/mw/com/impl/tracing:skeleton_tracing",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "find_service_handle",
    srcs = ["find_service_handle.cpp"],
    hdrs = ["find_service_handle.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score-baselibs//score/mw/log"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__pkg__",
        "//score/mw/com/impl:__subpackages__",
    ],
)

cc_library(
    name = "find_service_handler",
    srcs = ["find_service_handler.cpp"],
    hdrs = ["find_service_handler.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__pkg__",
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":find_service_handle",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_base",
    srcs = ["proxy_base.cpp"],
    hdrs = ["proxy_base.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":find_service_handle",
        ":find_service_handler",
        ":runtime",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/plumbing",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_event_base",
    srcs = ["proxy_event_base.cpp"],
    hdrs = ["proxy_event_base.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl/tracing:proxy_event_tracing",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__pkg__",
        "//score/mw/com/impl/rust:__pkg__",
    ],
    deps = [
        ":error",
        ":proxy_base",
        ":proxy_event_binding",
        ":sample_reference_tracker",
        "@score-baselibs//score/language/safecpp/scoped_function:scope",
        "//score/mw/com/impl/tracing:proxy_event_tracing_data",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "subscription_state",
    srcs = ["subscription_state.cpp"],
    hdrs = ["subscription_state.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__pkg__",
    ],
)

cc_library(
    name = "skeleton_binding",
    srcs = [
        "skeleton_binding.cpp",
    ],
    hdrs = [
        "skeleton_binding.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/mock_binding:__pkg__",
        "//score/mw/com/impl/plumbing:__subpackages__",
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":binding_type",
        "@score-baselibs//score/memory/shared:i_shared_memory_resource",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/configuration",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_event_binding",
    srcs = [
        "skeleton_event_binding.cpp",
    ],
    hdrs = [
        "skeleton_event_binding.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/mock_binding:__pkg__",
        "//score/mw/com/impl/plumbing:__subpackages__",
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":binding_type",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "//score/mw/com/impl/tracing:skeleton_event_tracing_data",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_binding",
    srcs = [
        "proxy_binding.cpp",
    ],
    hdrs = [
        "proxy_binding.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/mock_binding:__pkg__",
        "//score/mw/com/impl/plumbing:__subpackages__",
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        "proxy_event_binding",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_event_binding",
    srcs = [
        "proxy_event_binding.cpp",
        "proxy_event_binding_base.cpp",
    ],
    hdrs = [
        "proxy_event_binding.h",
        "proxy_event_binding_base.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/mock_binding:__pkg__",
        "//score/mw/com/impl/plumbing:__subpackages__",
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":binding_type",
        ":sample_reference_tracker",
        ":scoped_event_receive_handler",
        ":subscription_state",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "//score/mw/com/impl/tracing:i_tracing_runtime",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "generic_proxy_event_binding",
    srcs = [
        "generic_proxy_event_binding.cpp",
    ],
    hdrs = [
        "generic_proxy_event_binding.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/mock_binding:__pkg__",
        "//score/mw/com/impl/plumbing:__subpackages__",
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":binding_type",
        ":proxy_event_binding",
        ":scoped_event_receive_handler",
        ":subscription_state",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "//score/mw/com/impl/tracing:i_tracing_runtime",
    ],
)

cc_library(
    name = "handle_type",
    srcs = ["handle_type.cpp"],
    hdrs = ["handle_type.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":instance_identifier",
        "//score/mw/com/impl/configuration",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "instance_identifier",
    srcs = ["instance_identifier.cpp"],
    hdrs = ["instance_identifier.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":error",
        "//score/mw/com/impl/configuration",
    ],
)

cc_library(
    name = "enriched_instance_identifier",
    srcs = ["enriched_instance_identifier.cpp"],
    hdrs = ["enriched_instance_identifier.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":handle_type",
        ":instance_identifier",
        "//score/mw/com/impl/configuration:service_instance_id",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "error",
    srcs = ["com_error.cpp"],
    hdrs = ["com_error.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = ["@score-baselibs//score/result"],
)

cc_library(
    name = "instance_specifier",
    srcs = ["instance_specifier.cpp"],
    hdrs = ["instance_specifier.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":error",
        "@score-baselibs//score/result",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_service_discovery_client",
    srcs = ["i_service_discovery_client.cpp"],
    hdrs = ["i_service_discovery_client.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":enriched_instance_identifier",
        ":find_service_handler",
        ":handle_type",
        ":i_service_discovery",
        ":instance_identifier",
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "service_element_type",
    srcs = ["service_element_type.cpp"],
    hdrs = ["service_element_type.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/mw/log",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
        # "@ddad//score/mw/com:__subpackages__",
    ],
)

cc_library(
    name = "service_discovery_client_mock",
    testonly = True,
    srcs = ["service_discovery_client_mock.cpp"],
    hdrs = ["service_discovery_client_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":i_service_discovery_client",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "binding_type",
    srcs = ["binding_type.cpp"],
    hdrs = ["binding_type.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
)

cc_library(
    name = "runtime_interfaces",
    srcs = [
        "i_runtime.cpp",
        "i_runtime_binding.cpp",
    ],
    hdrs = [
        "i_runtime.h",
        "i_runtime_binding.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":binding_type",
        ":i_service_discovery",
        ":i_service_discovery_client",
        ":instance_identifier",
        ":instance_specifier",
        "//score/mw/com/impl/tracing:i_tracing_runtime",
        "//score/mw/com/impl/tracing:i_tracing_runtime_binding",
        "//score/mw/com/impl/tracing/configuration:i_tracing_filter_config",
    ],
)

cc_library(
    name = "runtime_mock",
    testonly = True,
    hdrs = ["runtime_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":runtime_interfaces",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "event_receive_handler",
    srcs = ["event_receive_handler.cpp"],
    hdrs = ["event_receive_handler.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = ["@score-baselibs//score/language/futurecpp"],
)

cc_library(
    name = "i_service_discovery",
    srcs = ["i_service_discovery.cpp"],
    hdrs = ["i_service_discovery.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":enriched_instance_identifier",
        ":find_service_handle",
        ":find_service_handler",
        ":handle_type",
        ":instance_identifier",
        ":instance_specifier",
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "service_discovery",
    srcs = ["service_discovery.cpp"],
    hdrs = ["service_discovery.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":enriched_instance_identifier",
        ":find_service_handle",
        ":find_service_handler",
        ":handle_type",
        ":i_service_discovery",
        ":instance_identifier",
        ":instance_specifier",
        ":runtime_interfaces",
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "scoped_event_receive_handler",
    srcs = ["scoped_event_receive_handler.cpp"],
    hdrs = ["scoped_event_receive_handler.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = ["@score-baselibs//score/language/safecpp/scoped_function:move_only_scoped_function"],
)

cc_library(
    name = "service_discovery_mock",
    testonly = True,
    srcs = ["service_discovery_mock.cpp"],
    hdrs = ["service_discovery_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":find_service_handle",
        ":find_service_handler",
        ":handle_type",
        ":i_service_discovery",
        ":instance_identifier",
        ":instance_specifier",
        "@score-baselibs//score/result",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "runtime",
    srcs = ["runtime.cpp"],
    hdrs = ["runtime.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":instance_identifier",
        ":instance_specifier",
        ":runtime_interfaces",
        ":service_discovery",
        "@score-baselibs//score/concurrency:long_running_threads_container",
        "@score-baselibs//score/memory:any_string_view",
        "@score-baselibs//score/memory/shared:types",
        "//score/mw/com:runtime_configuration",
        "//score/mw/com/impl:event_receive_handler",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/plumbing:runtime",
        "//score/mw/com/impl/tracing:tracing_runtime",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_parser",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "sample_reference_tracker",
    srcs = ["sample_reference_tracker.cpp"],
    hdrs = ["sample_reference_tracker.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/language/safecpp/safe_math",
        "@score-baselibs//score/language/futurecpp",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
)

cc_library(
    name = "service_element_map",
    srcs = ["service_element_map.cpp"],
    hdrs = ["service_element_map.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
)

cc_library(
    name = "flag_owner",
    srcs = ["flag_owner.cpp"],
    hdrs = ["flag_owner.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
)

cc_test(
    name = "unit_test_runtime_single_exec",
    timeout = "moderate",
    srcs = ["runtime_single_exec_test.cpp"],
    data = [
        "//score/mw/com/impl/configuration:ara_com_config.json",
        "//score/mw/com/impl/configuration:ara_com_config_disabled_trace_config.json",
        "//score/mw/com/impl/configuration:ara_com_config_invalid_trace_config_path.json",
        "//score/mw/com/impl/configuration:ara_com_config_other.json",
        "//score/mw/com/impl/configuration:ara_com_config_valid_trace_config.json",
        "//score/mw/com/impl/tracing/configuration:comtrace_filter_config_small.json",
    ],
    features = [
        "aborts_upon_exception",
    ],
    tags = ["unit"],
    deps = [
        ":impl",
        ":runtime",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "@score-baselibs//score/mw/log",
        "@googletest//:gtest_main",
    ],
)

cc_gtest_unit_test(
    name = "service_discovery_test",
    srcs = ["service_discovery_test.cpp"],
    deps = [
        ":impl",
        ":runtime_mock",
        ":service_discovery_client_mock",
        "//score/mw/com/impl/bindings/lola:runtime_mock",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "generic_proxy_test",
    srcs = ["generic_proxy_test.cpp"],
    deps = [
        ":impl",
        ":runtime_mock",
        ":service_discovery_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/test:binding_factory_resources",
        "//score/mw/com/impl/test:dummy_instance_identifier_builder",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "sample_reference_tracker_test",
    srcs = ["sample_reference_tracker_test.cpp"],
    deps = [
        ":sample_reference_tracker",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "flag_owner_test",
    srcs = ["flag_owner_test.cpp"],
    deps = [
        ":flag_owner",
    ],
)

cc_gtest_unit_test(
    name = "service_element_type_test",
    srcs = ["service_element_type_test.cpp"],
    deps = [
        ":service_element_type",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "runtime_test",
    srcs = ["runtime_test.cpp"],
    deps = [
        ":runtime",
        ":runtime_mock",
        "@score-baselibs//score/analysis/tracing/library/generic_trace_api/mocks:trace_library_mock",
        "//score/mw/com/impl/bindings/lola:runtime_mock",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/tracing:tracing_runtime",
        "//score/mw/com/impl/tracing:tracing_test_resources",
        "//score/mw/com/impl/tracing/configuration:proxy_event_trace_point_type",
        "//score/mw/com/impl/tracing/configuration:skeleton_event_trace_point_type",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "find_service_handle_test",
    srcs = ["find_service_handle_test.cpp"],
    deps = [
        ":find_service_handle",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/mw/log:recorder_mock",
    ],
)

cc_test(
    name = "unit_test",
    # TODO: Investigate why this unit tests takes more than 1 minute under memcheck (Ticket-141169)
    timeout = "moderate",
    srcs = [
        "com_error_test.cpp",
        "enriched_instance_identifier_test.cpp",
        "generic_proxy_event_test.cpp",
        "handle_type_test.cpp",
        "instance_identifier_test.cpp",
        "instance_specifier_test.cpp",
        "proxy_base_test.cpp",
        "proxy_binding_test.cpp",
        "proxy_event_base_test.cpp",
        "proxy_event_binding_base_test.cpp",
        "proxy_event_test.cpp",
        "proxy_field_test.cpp",
        "service_element_map_test.cpp",
        "skeleton_base_test.cpp",
        "skeleton_binding_test.cpp",
        "skeleton_event_base_test.cpp",
        "skeleton_event_binding_test.cpp",
        "skeleton_event_test.cpp",
        "skeleton_field_base_test.cpp",
        "skeleton_field_test.cpp",
        "traits_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    tags = ["unit"],
    deps = [
        ":enriched_instance_identifier",
        ":impl",
        ":runtime_mock",
        ":service_discovery_mock",
        ":service_element_map",
        "@score-baselibs//score/analysis/tracing/library/generic_trace_api/mocks:trace_library_mock",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "@score-baselibs//score/os/mocklib:mman_mock",
        "@score-baselibs//score/os/mocklib:stat_mock",
        "//score/mw/com:types",
        "//score/mw/com/impl/bindings/lola:runtime_mock",
        "//score/mw/com/impl/bindings/lola/test_doubles",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/configuration/test:configuration_test_resources",
        "//score/mw/com/impl/plumbing:proxy_binding_factory_mock",
        "//score/mw/com/impl/plumbing:proxy_event_binding_factory_mock",
        "//score/mw/com/impl/plumbing:proxy_field_binding_factory_mock",
        "//score/mw/com/impl/plumbing:skeleton_binding_factory_mock",
        "//score/mw/com/impl/plumbing:skeleton_event_binding_factory_mock",
        "//score/mw/com/impl/plumbing:skeleton_field_binding_factory_mock",
        "//score/mw/com/impl/test:binding_factory_resources",
        "//score/mw/com/impl/test:proxy_resources",
        "//score/mw/com/impl/tracing:tracing_test_resources",
        "@score-baselibs//score/mw/log",
        "@googletest//:gtest_main",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":service_element_type_test",
        ":flag_owner_test",
        ":generic_proxy_test",
        ":sample_reference_tracker_test",
        ":find_service_handle_test",
        ":service_discovery_test",
        ":unit_test",
        ":runtime_test",
        ":unit_test_runtime_single_exec",
    ],
    test_suites_from_sub_packages = [
        "//score/mw/com/impl/bindings/lola:unit_test_suite",
        "//score/mw/com/impl/configuration:unit_test_suite",
        "//score/mw/com/impl/plumbing:unit_test_suite",
        "//score/mw/com/impl/tracing:unit_test_suite",
    ],
    visibility = ["//score/mw/com:__pkg__"],
)
