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
load("@score-baselibs//bazel:unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "lola",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola/test:__pkg__",
        "//score/mw/com/impl/plumbing:__pkg__",
    ],
    deps = [
        ":event",
        ":proxy",
        ":runtime",
        ":skeleton",
        "@score-baselibs//score/concurrency",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/message_passing",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "path_builder",
    srcs = ["path_builder.cpp"],
    hdrs = ["path_builder.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = ["//score/mw/com/impl/configuration:lola_service_instance_id"],
)

cc_library(
    name = "shm_path_builder",
    srcs = ["shm_path_builder.cpp"],
    hdrs = ["shm_path_builder.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [":path_builder"],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/plumbing:__pkg__"],
    deps = [
        ":i_shm_path_builder",
        "//score/mw/com/impl/configuration",
    ],
)

cc_library(
    name = "partial_restart_path_builder",
    srcs = ["partial_restart_path_builder.cpp"],
    hdrs = ["partial_restart_path_builder.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [":path_builder"],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/plumbing:__pkg__"],
    deps = [":i_partial_restart_path_builder"],
)

cc_library(
    name = "i_runtime",
    srcs = ["i_runtime.cpp"],
    hdrs = ["i_runtime.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/plumbing:__pkg__"],
    deps = [
        ":rollback_synchronization",
        "//score/mw/com/impl:runtime_interfaces",
        "//score/mw/com/impl/bindings/lola/messaging",
        "//score/mw/com/impl/configuration",
    ],
)

cc_library(
    name = "i_shm_path_builder",
    srcs = ["i_shm_path_builder.cpp"],
    hdrs = ["i_shm_path_builder.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl/configuration",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_partial_restart_path_builder",
    srcs = ["i_partial_restart_path_builder.cpp"],
    hdrs = ["i_partial_restart_path_builder.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl/configuration",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "runtime",
    srcs = ["runtime.cpp"],
    hdrs = ["runtime.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/plumbing:__pkg__"],
    deps = [
        ":i_runtime",
        "@score-baselibs//score/concurrency:executor",
        "@score-baselibs//score/concurrency:long_running_threads_container",
        "@score-baselibs//score/memory/shared",
        "//score/mw/com/impl/bindings/lola/messaging",
        "//score/mw/com/impl/bindings/lola/service_discovery/client:service_discovery_client",
        "//score/mw/com/impl/bindings/lola/tracing:tracing_runtime",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "rollback_synchronization",
    srcs = ["rollback_synchronization.cpp"],
    hdrs = ["rollback_synchronization.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "runtime_mock",
    testonly = True,
    hdrs = ["runtime_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":i_runtime",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "shm_path_builder_mock",
    testonly = True,
    srcs = ["shm_path_builder_mock.cpp"],
    hdrs = ["shm_path_builder_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":i_shm_path_builder",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "partial_restart_path_builder_mock",
    testonly = True,
    srcs = ["partial_restart_path_builder_mock.cpp"],
    hdrs = ["partial_restart_path_builder_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":i_partial_restart_path_builder",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "event_meta_info",
    srcs = ["event_meta_info.cpp"],
    hdrs = ["event_meta_info.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":data_type_meta_info",
        "@score-baselibs//score/memory/shared",
    ],
)

cc_library(
    name = "data_type_meta_info",
    srcs = ["data_type_meta_info.cpp"],
    hdrs = ["data_type_meta_info.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
)

cc_library(
    name = "shared_data_structures",
    srcs = [
        "service_data_control.cpp",
        "service_data_storage.cpp",
    ],
    hdrs = [
        "service_data_control.h",
        "service_data_storage.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola/test_doubles:__pkg__",
    ],
    deps = [
        ":element_fq_id",
        ":event",
        ":event_meta_info",
        ":uid_pid_mapping",
        "@score-baselibs//score/memory/shared",
    ],
)

cc_library(
    name = "skeleton",
    srcs = [
        "skeleton.cpp",
        "skeleton_event.cpp",
        "skeleton_event_properties.cpp",
    ],
    hdrs = [
        "skeleton.h",
        "skeleton_event.h",
        "skeleton_event_properties.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola/test:__pkg__",
        "//score/mw/com/impl/bindings/lola/test_doubles:__pkg__",
    ],
    deps = [
        ":event",
        ":i_partial_restart_path_builder",
        ":i_shm_path_builder",
        ":partial_restart_path_builder",
        ":shared_data_structures",
        ":shm_path_builder",
        ":transaction_log_registration_guard",
        "@score-baselibs//score/language/safecpp/safe_math",
        "@score-baselibs//score/filesystem",
        "@score-baselibs//score/memory/shared",
        "@score-baselibs//score/memory/shared:lock_file",
        "@score-baselibs//score/memory/shared:new_delete_delegate_resource",
        "@score-baselibs//score/memory/shared/flock:exclusive_flock_mutex",
        "@score-baselibs//score/memory/shared/flock:flock_mutex_and_lock",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:skeleton_binding",
        "//score/mw/com/impl:skeleton_event_binding",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "//score/mw/com/impl/tracing:skeleton_event_tracing",
        "//score/mw/com/impl/util:arithmetic_utils",
    ],
)

cc_library(
    name = "proxy",
    srcs = [
        "generic_proxy_event.cpp",
        "proxy.cpp",
        "proxy_event.cpp",
        "proxy_event_common.cpp",
        "slot_collector.cpp",
        "subscription_helpers.cpp",
        "subscription_not_subscribed_states.cpp",
        "subscription_state_base.cpp",
        "subscription_state_machine.cpp",
        "subscription_state_machine_states.cpp",
        "subscription_subscribed_states.cpp",
        "subscription_subscription_pending_states.cpp",
    ],
    hdrs = [
        "generic_proxy_event.h",
        "proxy.h",
        "proxy_event.h",
        "proxy_event_common.h",
        "slot_collector.h",
        "subscription_helpers.h",
        "subscription_not_subscribed_states.h",
        "subscription_state_base.h",
        "subscription_state_machine.h",
        "subscription_state_machine_states.h",
        "subscription_subscribed_states.h",
        "subscription_subscription_pending_states.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola/test:__pkg__"],
    deps = [
        ":event",
        ":event_control",
        ":event_subscription_control",
        ":shared_data_structures",
        ":shm_path_builder",
        ":transaction_log_id",
        ":transaction_log_registration_guard",
        ":transaction_log_rollback_executor",
        "@score-baselibs//score/language/safecpp/safe_math",
        "@score-baselibs//score/filesystem",
        "@score-baselibs//score/memory/shared",
        "@score-baselibs//score/memory/shared:lock_file",
        "@score-baselibs//score/memory/shared:pointer_arithmetic_util",
        "@score-baselibs//score/memory/shared/flock:flock_mutex_and_lock",
        "@score-baselibs//score/memory/shared/flock:shared_flock_mutex",
        "@score-baselibs//score/os:errno_logging",
        "@score-baselibs//score/os:fcntl",
        "@score-baselibs//score/os:glob",
        "@score-baselibs//score/os:unistd",
        "@score-baselibs//score/result",
        "//score/mw/com/impl:generic_proxy_event_binding",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl:proxy_binding",
        "//score/mw/com/impl:proxy_event_binding",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:runtime_interfaces",
        "//score/mw/com/impl:scoped_event_receive_handler",
        "//score/mw/com/impl/bindings/lola:partial_restart_path_builder",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "//score/mw/com/impl/tracing:i_tracing_runtime",
        "@score-baselibs//score/mw/log",
    ],
)

cc_library(
    name = "element_fq_id",
    srcs = ["element_fq_id.cpp"],
    hdrs = ["element_fq_id.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__subpackages__",
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "@score-baselibs//score/mw/log",
    ],
)

cc_library(
    name = "sample_ptr",
    srcs = ["sample_ptr.cpp"],
    hdrs = ["sample_ptr.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":event_data_control",
        ":slot_decrementer",
    ],
)

cc_library(
    name = "slot_decrementer",
    srcs = ["slot_decrementer.cpp"],
    hdrs = ["slot_decrementer.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":event_data_control",
        ":transaction_log_set",
    ],
)

cc_library(
    name = "uid_pid_mapping",
    srcs = ["uid_pid_mapping.cpp"],
    hdrs = ["uid_pid_mapping.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score-baselibs//score/mw/log"],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        "@score-baselibs//score/containers:dynamic_array",
        "@score-baselibs//score/memory/shared:atomic_indirector",
        "@score-baselibs//score/memory/shared:types",
    ],
)

cc_library(
    name = "control_slot_indicator",
    srcs = [
        "control_slot_composite_indicator.cpp",
        "control_slot_indicator.cpp",
    ],
    hdrs = [
        "control_slot_composite_indicator.h",
        "control_slot_indicator.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        ":control_slot_types",
        ":event_slot_status",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "event_data_control",
    srcs = ["event_data_control.cpp"],
    hdrs = ["event_data_control.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        ":control_slot_indicator",
        ":control_slot_types",
        ":event_slot_status",
        ":transaction_log",
        ":transaction_log_id",
        ":transaction_log_set",
        "@score-baselibs//score/containers:dynamic_array",
        "@score-baselibs//score/memory/shared:atomic_indirector",
        "@score-baselibs//score/memory/shared:types",
    ],
)

cc_library(
    name = "control_slot_types",
    srcs = ["control_slot_types.cpp"],
    hdrs = ["control_slot_types.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":event_slot_status",
    ],
)

cc_library(
    name = "event_control",
    srcs = ["event_control.cpp"],
    hdrs = ["event_control.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        ":control_slot_types",
        ":event_data_control",
        ":event_subscription_control",
    ],
)

cc_library(
    name = "event_subscription_control",
    srcs = ["event_subscription_control.cpp"],
    hdrs = ["event_subscription_control.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/language/safecpp/safe_math",
        "@score-baselibs//score/mw/log",
    ],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        "@score-baselibs//score/memory/shared:atomic_indirector",
        "//score/mw/com/impl/configuration",
    ],
)

cc_library(
    name = "event_slot_status",
    srcs = ["event_slot_status.cpp"],
    hdrs = ["event_slot_status.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
)

cc_library(
    name = "event_data_control_composite",
    srcs = ["event_data_control_composite.cpp"],
    hdrs = ["event_data_control_composite.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":control_slot_types",
        ":event_data_control",
        ":event_slot_status",
        "@score-baselibs//score/memory/shared:atomic_indirector",
    ],
)

cc_library(
    name = "event_data_storage",
    srcs = ["event_data_storage.cpp"],
    hdrs = ["event_data_storage.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "@score-baselibs//score/containers:dynamic_array",
        "@score-baselibs//score/memory/shared:types",
    ],
)

cc_library(
    name = "sample_allocatee_ptr",
    srcs = ["sample_allocatee_ptr.cpp"],
    hdrs = ["sample_allocatee_ptr.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":control_slot_types",
        ":event_data_control_composite",
    ],
)

cc_library(
    name = "event",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
        "//score/mw/com/impl/bindings/lola/test:__pkg__",
        "//score/mw/com/impl/bindings/lola/test_doubles:__pkg__",
        "//score/mw/com/impl/plumbing:__pkg__",
    ],
    deps = [
        ":event_control",
        ":event_data_control",
        ":event_data_control_composite",
        ":event_data_storage",
        ":event_slot_status",
        ":event_subscription_control",
        ":sample_allocatee_ptr",
        ":sample_ptr",
    ],
)

cc_library(
    name = "transaction_log",
    srcs = ["transaction_log.cpp"],
    hdrs = ["transaction_log.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl:error",
        "@score-baselibs//score/mw/log",
    ],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        ":transaction_log_slot",
        "@score-baselibs//score/memory/shared",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "transaction_log_id",
    srcs = ["transaction_log_id.cpp"],
    hdrs = ["transaction_log_id.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
)

cc_library(
    name = "transaction_log_set",
    srcs = ["transaction_log_set.cpp"],
    hdrs = ["transaction_log_set.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        ":transaction_log",
        ":transaction_log_id",
        "@score-baselibs//score/containers:dynamic_array",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/util:copyable_atomic",
    ],
)

cc_library(
    name = "transaction_log_rollback_executor",
    srcs = ["transaction_log_rollback_executor.cpp"],
    hdrs = ["transaction_log_rollback_executor.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        ":shared_data_structures",
        ":transaction_log_id",
        ":transaction_log_set",
        "@score-baselibs//score/result",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl/bindings/lola:runtime",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "transaction_log_registration_guard",
    srcs = ["transaction_log_registration_guard.cpp"],
    hdrs = ["transaction_log_registration_guard.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl:error",
    ],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        ":event_data_control",
        ":transaction_log_id",
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "transaction_log_slot",
    srcs = ["transaction_log_slot.cpp"],
    hdrs = ["transaction_log_slot.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
)

cc_library(
    name = "event_data_control_test_resources",
    testonly = True,
    srcs = ["event_data_control_test_resources.cpp"],
    hdrs = ["event_data_control_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "//score/mw/com/impl/bindings/lola:event",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "partial_restart_path_builder_test",
    srcs = ["partial_restart_path_builder_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [":partial_restart_path_builder"],
)

cc_gtest_unit_test(
    name = "shm_path_builder_test",
    srcs = ["shm_path_builder_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [":shm_path_builder"],
)

cc_gtest_unit_test(
    name = "uid_pid_mapping_test",
    srcs = ["uid_pid_mapping_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":uid_pid_mapping",
        "@score-baselibs//score/memory/shared:atomic_indirector",
        "@score-baselibs//score/memory/shared:atomic_indirector_mock_binding",
    ],
)

cc_gtest_unit_test(
    name = "generic_proxy_event_test",
    srcs = ["generic_proxy_event_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":proxy",
        "@score-baselibs//score/memory:any_string_view",
        "//score/mw/com/impl/bindings/lola/test:proxy_event_test_resources",
    ],
)

cc_unit_test(
    name = "dynamic_array_bounds_checking_test",
    srcs = [
        "dynamic_array_bounds_checking_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
        "//score/mw/com/impl/bindings/lola:__subpackages__",
        # "@ddad//score/mw/com/impl/bindings/lola:__subpackages__",
    ],
    deps = [
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "@score-baselibs//score/containers:dynamic_array",
        "@score-baselibs//score/memory/shared:fake_memory_resources",
        "@score-baselibs//score/memory/shared:polymorphic_offset_ptr_allocator",
        "@score-baselibs//score/mw/log",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score-baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)

cc_test(
    name = "event_data_control_unit_tests",
    size = "small",
    timeout = "long",
    srcs = [
        "event_data_control_composite_test.cpp",
        "event_data_control_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    tags = ["unit"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "@score-baselibs//score/containers:dynamic_array",
        "@score-baselibs//score/memory/shared:atomic_indirector_mock_binding",
        "@score-baselibs//score/memory/shared:new_delete_delegate_resource",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl/bindings/lola:event_data_control",
        "//score/mw/com/impl/bindings/lola:event_data_control_composite",
        "//score/mw/com/impl/bindings/lola/test_doubles:fake_memory_resource",
        "@googletest//:gtest_main",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_test(
    name = "skeleton_test",
    timeout = "long",  # Investigate why tests timeout and mark as cc_gtest_unit_test in Ticket-178974
    srcs = ["skeleton_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    tags = ["unit"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":skeleton",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "//score/mw/com/impl/bindings/lola/test:skeleton_test_resources",
        "//score/mw/com/impl/bindings/lola/test:transaction_log_test_resources",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "@googletest//:gtest_main",
    ],
)

cc_gtest_unit_test(
    name = "proxy_test",
    srcs = ["proxy_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":proxy",
        ":shm_path_builder_mock",
        "@score-baselibs//score/memory:any_string_view",
        "@score-baselibs//score/result",
        "//score/mw/com/impl/bindings/lola/test:proxy_event_test_resources",
        "//score/mw/com/impl/bindings/lola/test:transaction_log_test_resources",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/configuration",
    ],
)

cc_gtest_unit_test(
    name = "slot_decrementer_test",
    srcs = ["slot_decrementer_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":event_data_control",
        ":slot_decrementer",
        "//score/mw/com/impl/bindings/lola/test_doubles:fake_memory_resource",
    ],
)

cc_test(
    name = "unit_test",
    size = "small",
    timeout = "long",
    srcs = [
        "control_slot_composite_indicator_test.cpp",
        "control_slot_indicator_test.cpp",
        "element_fq_id_test.cpp",
        "event_data_storage_test.cpp",
        "event_slot_status_test.cpp",
        "event_subscription_control_test.cpp",
        "proxy_event_common_test.cpp",
        "proxy_event_test.cpp",
        "runtime_test.cpp",
        "sample_allocatee_ptr_test.cpp",
        "sample_ptr_test.cpp",
        "service_data_storage_test.cpp",
        "skeleton_event_test.cpp",
        "skeleton_event_tracing_test.cpp",
        "slot_collector_test.cpp",
        "subscription_state_machine_events_test.cpp",
        "subscription_state_machine_methods_test.cpp",
        "subscription_state_machine_states_test.cpp",
        "transaction_log_registration_guard_test.cpp",
        "transaction_log_rollback_executor_test.cpp",
        "transaction_log_set_test.cpp",
        "transaction_log_slot_test.cpp",
        "transaction_log_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    tags = ["unit"],
    visibility = [
        # "@ddad//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":event_data_control_test_resources",
        ":lola",
        ":shm_path_builder_mock",
        ":transaction_log",
        ":transaction_log_id",
        ":transaction_log_rollback_executor",
        ":transaction_log_set",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "@score-baselibs//score/concurrency:long_running_threads_container",
        "@score-baselibs//score/filesystem",
        "@score-baselibs//score/filesystem:mock",
        "@score-baselibs//score/memory/shared",
        "@score-baselibs//score/memory/shared:atomic_indirector_mock_binding",
        "@score-baselibs//score/memory/shared:shared_memory_factory_mock",
        "@score-baselibs//score/memory/shared:shared_memory_resource_heap_allocator_mock",
        "@score-baselibs//score/memory/shared:shared_memory_resource_mock",
        "@score-baselibs//score/os/mocklib:fcntl_mock",
        "@score-baselibs//score/os/mocklib:glob_mock",
        "@score-baselibs//score/os/mocklib:grp_mock",
        "@score-baselibs//score/os/mocklib:mqueue_mock",
        "@score-baselibs//score/os/mocklib:stat_mock",
        "@score-baselibs//score/os/mocklib:stdlib_mock",
        "@score-baselibs//score/os/mocklib:unistd_mock",
        "@score-baselibs//score/os/utils/inotify:inotify_instance_facade",
        "@score-baselibs//score/os/utils/inotify:inotify_instance_mock",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/lola:runtime_mock",
        "//score/mw/com/impl/bindings/lola/messaging:mock",
        "//score/mw/com/impl/bindings/lola/test:proxy_event_test_resources",
        "//score/mw/com/impl/bindings/lola/test:service_discovery_test_resources",
        "//score/mw/com/impl/bindings/lola/test:skeleton_event_test_resources",
        "//score/mw/com/impl/bindings/lola/test:skeleton_test_resources",
        "//score/mw/com/impl/bindings/lola/test:transaction_log_test_resources",
        "//score/mw/com/impl/bindings/lola/test_doubles",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/tracing:skeleton_event_tracing",
        "//score/mw/com/impl/tracing:skeleton_event_tracing_data",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
        "//score/mw/com/message_passing:mock",
        "@score-baselibs//score/mw/log",
        "@googletest//:gtest_main",
    ],
)

test_suite(
    name = "unit_test_suite",
    tests = [
        ":dynamic_array_bounds_checking_test",
        ":event_data_control_unit_tests",
        ":generic_proxy_event_test",
        ":partial_restart_path_builder_test",
        ":proxy_test",
        ":shm_path_builder_test",
        ":skeleton_test",
        ":slot_decrementer_test",
        ":uid_pid_mapping_test",
        ":unit_test",
        "//score/mw/com/impl/bindings/lola/messaging:unit_test_suite",
        "//score/mw/com/impl/bindings/lola/service_discovery:unit_test_suite",
        "//score/mw/com/impl/bindings/lola/test:proxy_component_test",
        "//score/mw/com/impl/bindings/lola/test:skeleton_component_test",
        "//score/mw/com/impl/bindings/lola/test:skeleton_event_component_test",
        "//score/mw/com/impl/bindings/lola/tracing:unit_test_suite",
    ],
    visibility = [
        "//score/mw/com/impl:__pkg__",
        # "@ddad//score/mw/com/impl:__pkg__",
    ],
)
