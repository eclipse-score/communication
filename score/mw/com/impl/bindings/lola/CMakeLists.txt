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

# Add subdirectories
add_subdirectory(messaging)
add_subdirectory(service_discovery)
add_subdirectory(tracing lola_tracing_build)
add_subdirectory(test lola_test_build)
add_subdirectory(test_doubles lola_test_doubles_build)

# Event data control library
add_library(com_impl_bindings_lola_event_data_control
    event_data_control.cpp
    event_data_control.h
    # Control slot components
    control_slot_indicator.cpp
    control_slot_composite_indicator.cpp
    control_slot_types.cpp
    # Event slot components
    event_slot_status.cpp
    # Transaction log components
    transaction_log.cpp
    transaction_log_set.cpp
    transaction_log_id.cpp
    transaction_log_slot.cpp
    transaction_log_registration_guard.cpp
    transaction_log_rollback_executor.cpp
    rollback_synchronization.cpp
    # Application ID and PID mapping (needed by transaction log)
    application_id_pid_mapping.cpp
    application_id_pid_mapping_entry.cpp
    register_pid_fake.cpp
)

target_include_directories(com_impl_bindings_lola_event_data_control
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(com_impl_bindings_lola_event_data_control
    PUBLIC
        score::score_memory_shared_region
        score::score_containers
)

# Event data control composite library
add_library(com_impl_bindings_lola_event_data_control_composite
    event_data_control_composite.cpp
)

target_link_libraries(com_impl_bindings_lola_event_data_control_composite
    PUBLIC
        com_impl_bindings_lola_event_data_control
        score::score_memory_shared_region
        score::score_containers
)

# Main LOLA bindings library with core implementations

# LOLA core binding implementation
add_library(com_impl_bindings_lola
    # Core LOLA binding files that actually exist
    data_type_meta_info.cpp
    element_fq_id.cpp
    event_control.cpp
    event_data_control_composite.cpp
    event_data_storage.cpp
    event_meta_info.cpp
    event_subscription_control.cpp
    generic_proxy_event.cpp
    i_partial_restart_path_builder.cpp
    i_runtime.cpp
    i_shm_path_builder.cpp
    partial_restart_path_builder.cpp
    partial_restart_path_builder_mock.cpp
    path_builder.cpp
    proxy.cpp
    proxy_event.cpp
    proxy_event_common.cpp
    runtime.cpp
    sample_ptr.cpp
    service_data_control.cpp
    service_data_storage.cpp
    shm_path_builder.cpp
    shm_path_builder_mock.cpp
    skeleton.cpp
    skeleton_event.cpp
    skeleton_event_properties.cpp
    slot_collector.cpp
    slot_decrementer.cpp
    subscription_helpers.cpp
    subscription_not_subscribed_states.cpp
    subscription_state_base.cpp
    subscription_state_machine.cpp
    subscription_state_machine_states.cpp
    subscription_subscribed_states.cpp
    subscription_subscription_pending_states.cpp
    type_erased_sample_ptrs_guard.cpp
)

target_include_directories(com_impl_bindings_lola
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_bindings_lola
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            # Core headers
            runtime.h
            i_runtime.h
            proxy.h
            skeleton.h
            
            # Event system headers
            event_control.h
            event_data_control.h
            event_data_control_composite.h
            event_data_storage.h
            event_slot_status.h
            event_subscription_control.h
            event_meta_info.h
            
            # Proxy/skeleton event headers
            proxy_event.h
            proxy_event_common.h
            generic_proxy_event.h
            skeleton_event.h
            skeleton_event_properties.h
            
            # Control slot headers
            control_slot_indicator.h
            control_slot_composite_indicator.h
            control_slot_types.h
            
            # Transaction logging headers
            transaction_log.h
            transaction_log_id.h
            transaction_log_registration_guard.h
            transaction_log_rollback_executor.h
            transaction_log_set.h
            transaction_log_slot.h
            
            # Memory management headers
            sample_allocatee_ptr.h
            sample_ptr.h
            slot_decrementer.h
            slot_collector.h
            type_erased_sample_ptrs_guard.h
            
            # Service data headers
            service_data_control.h
            service_data_storage.h
            
            # Path builder headers
            path_builder.h
            shm_path_builder.h
            partial_restart_path_builder.h
            i_shm_path_builder.h
            i_partial_restart_path_builder.h
            
            # Subscription headers
            subscription_helpers.h
            subscription_not_subscribed_states.h
            subscription_state_base.h
            subscription_state_machine.h
            subscription_state_machine_states.h
            subscription_subscribed_states.h
            subscription_subscription_pending_states.h
            
            # Utility headers
            element_fq_id.h
            data_type_meta_info.h
            application_id_pid_mapping.h
            application_id_pid_mapping_entry.h
            register_pid_fake.h

            rollback_synchronization.h
)

# Link libraries for LOLA bindings
target_link_libraries(com_impl_bindings_lola
    PRIVATE
        com_impl_bindings_lola_event_data_control
        com_impl_bindings_lola_event_data_control_composite
        com_impl_bindings_lola_service_discovery
        com_impl_bindings_lola_messaging_messages
        com_impl_bindings_lola_messaging
        com_impl_bindings_lola_tracing
        lola_service_discovery_client
        score::score_memory_shared_factory
        score::score_memory_shared_resource
        score::score_memory_shared_flock
        score::score_memory_shared_sealedshm
        score::score_memory_shared_typedshm
        score::score_filesystem
        score::score_memory_split_string_view
        score::score_mw_log_configuration
        score::score_mw_log_detail
        score::score_mw_log_core
        com_impl_configuration_test_resources
        acl  # System ACL library
        ${Boost_LIBRARIES}
)

# Add aliases for exported targets
add_library(score-communication::com_impl_bindings_lola ALIAS com_impl_bindings_lola)
add_library(score-communication::com_impl_bindings_lola_event_data_control ALIAS com_impl_bindings_lola_event_data_control)
add_library(score-communication::com_impl_bindings_lola_event_data_control_composite ALIAS com_impl_bindings_lola_event_data_control_composite)
add_library(score-communication::com_impl_bindings_lola_service_discovery ALIAS com_impl_bindings_lola_service_discovery)
add_library(score-communication::com_impl_bindings_lola_messaging ALIAS com_impl_bindings_lola_messaging)
add_library(score-communication::com_impl_bindings_lola_messaging_messages ALIAS com_impl_bindings_lola_messaging_messages)
add_library(score-communication::com_impl_bindings_lola_tracing ALIAS com_impl_bindings_lola_tracing)
add_library(score-communication::lola_service_discovery_client ALIAS lola_service_discovery_client)

# Install targets
install(TARGETS com_impl_bindings_lola
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/bindings/lola
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install event data control libraries (internal, no headers exported)
install(TARGETS 
    com_impl_bindings_lola_event_data_control
    com_impl_bindings_lola_event_data_control_composite
    EXPORT score-communicationTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
