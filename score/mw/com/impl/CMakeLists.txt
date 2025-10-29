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

# Note: This is a simplified implementation of the very complex impl module
# For a full implementation, each individual library would need to be created
# as shown in the Bazel BUILD file

# Add subdirectories for major components
add_subdirectory(configuration)
add_subdirectory(plumbing)
add_subdirectory(bindings)
add_subdirectory(tracing)

# Error handling library
add_library(com_impl_error
    com_error.cpp
)

target_include_directories(com_impl_error
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_error
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl
        FILES
            com_error.h
)

target_link_libraries(com_impl_error
    PUBLIC
        score::score_containers
        score::score_language
)

# Event receive handler
add_library(com_impl_event_receive_handler
    event_receive_handler.cpp
)

target_include_directories(com_impl_event_receive_handler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_event_receive_handler
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl
        FILES
            event_receive_handler.h
)

target_link_libraries(com_impl_event_receive_handler
    PUBLIC
        score::score_containers
        score::score_language
)

# Utility library
add_library(com_impl_util
    util/arithmetic_utils.cpp
    util/copyable_atomic.cpp
)

target_include_directories(com_impl_util
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_util
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl
        FILES
            util/arithmetic_utils.h
            util/copyable_atomic.h
)

target_link_libraries(com_impl_util
    PUBLIC
        score::score_containers
        score::score_language
)

# Runtime implementation
add_library(com_impl_runtime
    runtime.cpp
    i_runtime.cpp
    i_runtime_binding.cpp
    service_discovery.cpp
    i_service_discovery.cpp
    i_service_discovery_client.cpp
)

target_include_directories(com_impl_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_runtime
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl
        FILES
            runtime.h
            i_runtime.h
            i_runtime_binding.h
            service_discovery.h
            i_service_discovery.h
            i_service_discovery_client.h
)

target_link_libraries(com_impl_runtime
    PUBLIC
        com_impl_error
        com_impl_configuration
        com_impl_plumbing
        com_impl_tracing
        com_runtime_configuration
        message_passing
        score::score_containers
        score::score_language
        score::score_mw
        score::score_result
        score::score_concurrency
        score::score_memory_shared_resource
        score::score_memory_shared_containers
        score::score_memory_shared_factory
        score::score_memory_shared_interfaces
        score::score_memory_shared_region
        score::score_memory_split_string_view
        score::score_os
        score::score_mw_log_core
        score::score_mw_log_detail
        score::score_mw_log_configuration
)

# Core implementation components (simplified)
add_library(com_impl_core
    binding_type.cpp
    enriched_instance_identifier.cpp
    find_service_handle.cpp
    find_service_handler.cpp
    flag_owner.cpp
    handle_type.cpp
    instance_identifier.cpp
    instance_specifier.cpp
    proxy_base.cpp
    proxy_binding.cpp
    proxy_event.cpp
    proxy_event_base.cpp
    proxy_event_binding.cpp
    proxy_event_binding_base.cpp
    proxy_field.cpp
    skeleton_base.cpp
    skeleton_binding.cpp
    skeleton_event.cpp
    skeleton_event_base.cpp
    skeleton_event_binding.cpp
    skeleton_field.cpp
    skeleton_field_base.cpp
    sample_reference_tracker.cpp
    scoped_event_receive_handler.cpp
    service_element_map.cpp
    service_element_type.cpp
    subscription_state.cpp
    traits.cpp
)

target_include_directories(com_impl_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_core
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl
        FILES
            binding_type.h
            enriched_instance_identifier.h
            find_service_handle.h
            find_service_handler.h
            flag_owner.h
            handle_type.h
            instance_identifier.h
            instance_specifier.h
            proxy_base.h
            proxy_binding.h
            proxy_event.h
            proxy_event_base.h
            proxy_event_binding.h
            proxy_event_binding_base.h
            proxy_field.h
            skeleton_base.h
            skeleton_binding.h
            skeleton_event.h
            skeleton_event_base.h
            skeleton_event_binding.h
            skeleton_field.h
            skeleton_field_base.h
            sample_reference_tracker.h
            scoped_event_receive_handler.h
            service_element_map.h
            service_element_type.h
            subscription_state.h
            traits.h
)

target_link_libraries(com_impl_core
    PUBLIC
        com_impl_error
        com_impl_event_receive_handler
        com_impl_runtime
        com_impl_configuration
        com_impl_plumbing
        com_impl_bindings
        com_impl_tracing
        score::score_containers
        score::score_language
        score::score_memory
        score::score_mw
        score::score_result
        score::score_concurrency
        score::score_memory_shared_resource
        score::score_memory_shared_containers
        score::score_memory_shared_factory
        score::score_memory_shared_interfaces
        score::score_memory_shared_region
        score::score_memory_split_string_view
        score::score_os
)

# Generic proxy and proxy event (high-level components)
add_library(com_impl_generic_proxy
    generic_proxy.cpp
    generic_proxy_event.cpp
    generic_proxy_event_binding.cpp
)

target_include_directories(com_impl_generic_proxy
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_generic_proxy
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl
        FILES
            generic_proxy.h
            generic_proxy_event.h
            generic_proxy_event_binding.h
)

target_link_libraries(com_impl_generic_proxy
    PUBLIC
        com_impl_core
)

# Main impl aggregation library
add_library(com_impl INTERFACE)

target_link_libraries(com_impl
    INTERFACE
        com_impl_generic_proxy
        com_impl_core
        com_impl_runtime
        com_impl_error
        com_impl_event_receive_handler
)

# Add aliases for exported targets
add_library(score-communication::com_impl ALIAS com_impl)
add_library(score-communication::com_impl_core ALIAS com_impl_core)
add_library(score-communication::com_impl_runtime ALIAS com_impl_runtime)
add_library(score-communication::com_impl_error ALIAS com_impl_error)
add_library(score-communication::com_impl_event_receive_handler ALIAS com_impl_event_receive_handler)
add_library(score-communication::com_impl_util ALIAS com_impl_util)
add_library(score-communication::com_impl_generic_proxy ALIAS com_impl_generic_proxy)

# Mock library for testing
if(SCORE_BUILD_TESTING)
    add_library(com_impl_runtime_mock
        runtime_mock.h
        service_discovery_client_mock.cpp
        service_discovery_mock.cpp
    )

    target_include_directories(com_impl_runtime_mock
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
    )

    target_sources(com_impl_runtime_mock
        PUBLIC
            FILE_SET HEADERS
            BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl
            FILES
                runtime_mock.h
                service_discovery_client_mock.h
                service_discovery_mock.h
    )

    target_link_libraries(com_impl_runtime_mock
        PUBLIC
            com_impl_runtime
            GTest::gmock
    )

    # Tests for impl components
    add_executable(com_impl_test
        com_error_test.cpp
        enriched_instance_identifier_test.cpp
        find_service_handle_test.cpp
        flag_owner_test.cpp
        generic_proxy_test.cpp
        generic_proxy_event_test.cpp
        handle_type_test.cpp
        instance_identifier_test.cpp
        instance_specifier_test.cpp
        proxy_base_test.cpp
        proxy_binding_test.cpp
        proxy_event_base_test.cpp
        proxy_event_binding_base_test.cpp
        proxy_event_test.cpp
        proxy_field_test.cpp
        runtime_single_exec_test.cpp
        runtime_test.cpp
        sample_reference_tracker_test.cpp
        service_discovery_test.cpp
        service_element_map_test.cpp
        service_element_type_test.cpp
        skeleton_base_test.cpp
        skeleton_binding_test.cpp
        skeleton_event_base_test.cpp
        skeleton_event_binding_test.cpp
        skeleton_event_test.cpp
        skeleton_field_base_test.cpp
        skeleton_field_test.cpp
        traits_test.cpp
        # Test helper files
        test/dummy_instance_identifier_builder.cpp
        test/binding_factory_resources.cpp
        test/proxy_resources.cpp
    )

    target_link_libraries(com_impl_test
        PRIVATE
            com_impl
            com_impl_runtime_mock
            com_runtime_configuration
            score::score_filesystem
            score::score_filesystem_factory_fake
            score::score_memory_split_string_view
            score::score_os
            score::score_os_fake
            score::score_mw_log_configuration
            score::score_mw_log_detail
            score::score_mw_log_core
            GTest::gtest_main
    )

    gtest_discover_tests(com_impl_test)
endif()

# Install targets
install(TARGETS
    com_impl_error
    com_impl_event_receive_handler
    com_impl_util
    com_impl_runtime
    com_impl_core
    com_impl_generic_proxy
    com_impl
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(SCORE_BUILD_TESTING)
    install(TARGETS com_impl_runtime_mock
        EXPORT score-communicationTargets
        FILE_SET HEADERS DESTINATION include/score/mw/com/impl/mock
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()