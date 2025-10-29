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

# Plumbing library - low-level communication infrastructure
add_library(com_impl_plumbing
    runtime_binding_factory.cpp
    proxy_binding_factory.cpp
    proxy_binding_factory_impl.cpp
    proxy_event_binding_factory.cpp
    proxy_event_binding_factory_impl.cpp
    proxy_field_binding_factory.cpp
    proxy_field_binding_factory_impl.cpp
    proxy_service_element_binding_factory_impl.cpp
    skeleton_binding_factory.cpp
    skeleton_binding_factory_impl.cpp
    skeleton_event_binding_factory.cpp
    skeleton_event_binding_factory_impl.cpp
    skeleton_field_binding_factory.cpp
    skeleton_field_binding_factory_impl.cpp
    skeleton_service_element_binding_factory_impl.cpp
    sample_allocatee_ptr.cpp
    sample_ptr.cpp
    i_proxy_binding_factory.cpp
    i_proxy_event_binding_factory.cpp
    i_proxy_field_binding_factory.cpp
    i_skeleton_binding_factory.cpp
    i_skeleton_event_binding_factory.cpp
    i_skeleton_field_binding_factory.cpp
)

target_include_directories(com_impl_plumbing
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_plumbing
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/plumbing
        FILES
            runtime_binding_factory.h
            proxy_binding_factory.h
            proxy_binding_factory_impl.h
            proxy_event_binding_factory.h
            proxy_event_binding_factory_impl.h
            proxy_field_binding_factory.h
            proxy_field_binding_factory_impl.h
            proxy_service_element_binding_factory_impl.h
            skeleton_binding_factory.h
            skeleton_binding_factory_impl.h
            skeleton_event_binding_factory.h
            skeleton_event_binding_factory_impl.h
            skeleton_field_binding_factory.h
            skeleton_field_binding_factory_impl.h
            skeleton_service_element_binding_factory_impl.h
            sample_allocatee_ptr.h
            sample_ptr.h
            i_proxy_binding_factory.h
            i_proxy_event_binding_factory.h
            i_proxy_field_binding_factory.h
            i_skeleton_binding_factory.h
            i_skeleton_event_binding_factory.h
            i_skeleton_field_binding_factory.h
)

target_link_libraries(com_impl_plumbing
    PUBLIC
        message_passing
        score::score_language
        score::score_mw
        score::score_memory
        score::score_memory_shared_containers
        score::score_analysis_tracing_interface_types
        score::score_analysis_tracing_common_interface_types
        com_impl_runtime
        # LOLA dependencies needed by plumbing for runtime binding factory
        com_impl_bindings_lola_tracing
        com_impl_bindings_lola_messaging
)

# Add alias for exported target
add_library(score-communication::com_impl_plumbing ALIAS com_impl_plumbing)

# Install targets
install(TARGETS com_impl_plumbing
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/plumbing
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
