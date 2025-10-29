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

# Tracing library - handles distributed tracing
add_library(com_impl_tracing STATIC
    # Core tracing sources
    common_event_tracing.cpp
    proxy_event_tracing.cpp
    proxy_event_tracing_data.cpp
    skeleton_event_tracing.cpp
    skeleton_event_tracing_data.cpp
    skeleton_tracing.cpp
    service_element_tracing_data.cpp
    trace_error.cpp
    tracing_runtime.cpp
    i_tracing_runtime.cpp
    i_tracing_runtime_binding.cpp
    type_erased_sample_ptr.cpp
    tracing_test_resources.cpp
    
    # Configuration sources
    configuration/service_element_identifier.cpp
    configuration/service_element_identifier_view.cpp
    configuration/service_element_instance_identifier_view.cpp
    configuration/proxy_event_trace_point_type.cpp
    configuration/proxy_field_trace_point_type.cpp
    configuration/skeleton_event_trace_point_type.cpp
    configuration/skeleton_field_trace_point_type.cpp
    configuration/trace_point_key.cpp
    configuration/tracing_config.cpp
    configuration/tracing_filter_config.cpp
    configuration/tracing_filter_config_parser.cpp
    configuration/hash_helper_for_service_element_and_se_view.cpp
    configuration/hash_helper_utility.cpp
    configuration/i_tracing_filter_config.cpp
)

target_include_directories(com_impl_tracing
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_tracing
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/tracing
        FILES
            proxy_event_tracing.h
            tracing_runtime.h
            type_erased_sample_ptr.h
            common_event_tracing.h
            configuration/tracing_filter_config.h
            configuration/tracing_filter_config_parser.h
            configuration/service_element_identifier.h
            configuration/service_element_identifier_view.h
            configuration/service_element_instance_identifier_view.h
            configuration/trace_point_key.h
            configuration/proxy_event_trace_point_type.h
            configuration/proxy_field_trace_point_type.h
            configuration/skeleton_event_trace_point_type.h
            configuration/skeleton_field_trace_point_type.h
            configuration/tracing_config.h
            configuration/hash_helper_for_service_element_and_se_view.h
            configuration/hash_helper_utility.h
            configuration/i_tracing_filter_config.h
            proxy_event_tracing_data.h
            skeleton_event_tracing.h
            skeleton_event_tracing_data.h
            service_element_tracing_data.h
            trace_error.h
            i_tracing_runtime.h
            i_tracing_runtime_binding.h
)

target_link_libraries(com_impl_tracing
    PUBLIC
        com_impl_core
        score::score_mw
        score::score_language
        score::score_memory
        score::score_memory_split_string_view
        score::score_analysis_tracing_common_interface_types
        score::score_analysis_tracing_interface_types
        score::score_analysis_tracing_error_code
        score::score_analysis_tracing_chunk_list
        score::score_analysis_tracing_stub
        score::score_mw_log_core
        score::score_mw_log_detail
        score::score_mw_log_configuration
        score::score_os
)

# Add alias for exported target
add_library(score-communication::com_impl_tracing ALIAS com_impl_tracing)

# Install targets
install(TARGETS com_impl_tracing
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/tracing
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)