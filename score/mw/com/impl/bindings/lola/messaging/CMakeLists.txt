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
add_subdirectory(messages)

# LOLA messaging implementation
add_library(com_impl_bindings_lola_messaging
    # Message passing control
    i_message_passing_control.cpp
    i_message_passing_service.cpp
    message_passing_control.cpp
    message_passing_service.cpp
    message_passing_service_instance.cpp
    message_passing_facade.cpp
    message_passing_client_cache.cpp
    
    # Event handling
    i_notify_event_handler.cpp
    notify_event_handler.cpp
    
    # Threading
    thread_abstraction.cpp
    
    # Node management
    node_identifier_copier.cpp
    
    # Core types needed by messaging
    ../element_fq_id.cpp
)

target_include_directories(com_impl_bindings_lola_messaging
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_bindings_lola_messaging
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/bindings/lola/messaging
        FILES
            i_message_passing_control.h
            i_message_passing_service.h
            message_passing_control.h
            message_passing_service.h
            message_passing_service_instance.h
            message_passing_facade.h
            message_passing_client_cache.h
            i_notify_event_handler.h
            notify_event_handler.h
            thread_abstraction.h
            node_identifier_copier.h
)

target_link_libraries(com_impl_bindings_lola_messaging
    PUBLIC
        com_impl_bindings_lola_messaging_messages
        com_message_passing
        score::score_containers
        score::score_language
        score::score_mw
        score::score_concurrency
        score::score_executor
        score::score_mw_log_core
)

# Install targets
install(TARGETS com_impl_bindings_lola_messaging
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/bindings/lola/messaging
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)