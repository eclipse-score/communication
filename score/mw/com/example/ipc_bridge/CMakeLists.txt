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

# DataType library
add_library(ipc_bridge_datatype
    datatype.cpp
)

target_include_directories(ipc_bridge_datatype
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(ipc_bridge_datatype
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/datatype.h
)

target_link_libraries(ipc_bridge_datatype
    PUBLIC
        com
        score::score_language
)

# Sample sender receiver library
add_library(ipc_bridge_sample_sender_receiver
    sample_sender_receiver.cpp
)

target_include_directories(ipc_bridge_sample_sender_receiver
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(ipc_bridge_sample_sender_receiver
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/sample_sender_receiver.h
)

target_link_libraries(ipc_bridge_sample_sender_receiver
    PUBLIC
        ipc_bridge_datatype
        com
        score::score_mw
)

# IPC Bridge CPP executable
add_executable(ipc_bridge_cpp
    main.cpp
    assert_handler.cpp
)

target_include_directories(ipc_bridge_cpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ipc_bridge_cpp
    PRIVATE
        ipc_bridge_sample_sender_receiver
        com
        com_message_passing
        message_passing_common
        message_passing_unix_domain
        # LOLA libraries providing symbols must come after libraries that need them
        com_impl_bindings_lola_messaging
        com_impl_bindings_lola_tracing
        com_impl_tracing
        Boost::program_options
        score::score_language
        score::score_mw
        score::score_mw_log_detail
        score::score_mw_log_configuration
        score::score_mw_log_core
        score::score_filesystem
        score::score_memory_shared_resource
        score::score_memory_shared_sync
        score::score_memory_shared_sealedshm
        score::score_memory_shared_flock
        acl
)

# Copy configuration file to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/etc/mw_com_config.json
    ${CMAKE_CURRENT_BINARY_DIR}/mw_com_config.json
    COPYONLY
)

# Install targets
install(TARGETS
    ipc_bridge_datatype
    ipc_bridge_sample_sender_receiver
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/example/ipc_bridge
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install executable without headers
install(TARGETS
    ipc_bridge_cpp
    EXPORT score-communicationTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install entire etc directory alongside the executable
install(DIRECTORY etc
    DESTINATION bin
    FILES_MATCHING PATTERN "*"
)