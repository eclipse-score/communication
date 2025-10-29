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

# Add impl subdirectory first (it contains the implementation)
add_subdirectory(impl)

# Add message_passing subdirectory (needed by LOLA messaging bindings)
add_subdirectory(message_passing)

# Add example subdirectory for examples like ipc_bridge
add_subdirectory(example)

# COM Error Domain library
add_library(com_error_domain
    com_error_domain.cpp
)

target_include_directories(com_error_domain
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_error_domain
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com
        FILES
            com_error_domain.h
)

target_link_libraries(com_error_domain
    PUBLIC
        com_impl_error
)

# Types library
add_library(com_types
    types.cpp
)

target_include_directories(com_types
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_types
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com
        FILES
            types.h
)

target_link_libraries(com_types
    PUBLIC
        com_impl
        com_impl_event_receive_handler
)

# Runtime Configuration library
add_library(com_runtime_configuration
    runtime_configuration.cpp
)

target_include_directories(com_runtime_configuration
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_runtime_configuration
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com
        FILES
            runtime_configuration.h
)

target_link_libraries(com_runtime_configuration
    PUBLIC
        score::score_filesystem
        score::score_language
        score::score_memory
        score::score_mw
)

# Runtime library
add_library(com_runtime
    runtime.cpp
)

target_include_directories(com_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_runtime
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com
        FILES
            runtime.h
)

target_link_libraries(com_runtime
    PUBLIC
        com_runtime_configuration
        com_types
        com_impl_runtime
    PRIVATE
        score::score_language
)

# Main COM library (public interface)
add_library(com INTERFACE)

target_link_libraries(com
    INTERFACE
        com_error_domain
        com_runtime
        com_runtime_configuration
        com_types
)

# Add aliases for exported targets
add_library(score-communication::com ALIAS com)
add_library(score-communication::com_error_domain ALIAS com_error_domain)
add_library(score-communication::com_types ALIAS com_types)
add_library(score-communication::com_runtime_configuration ALIAS com_runtime_configuration)
add_library(score-communication::com_runtime ALIAS com_runtime)

# Tests
if(SCORE_BUILD_TESTING)
    add_executable(com_unit_test
        runtime_test.cpp
        runtime_configuration_test.cpp
        types_accessibility_test.cpp
        types_test.cpp
    )

    target_link_libraries(com_unit_test
        PRIVATE
            com_runtime
            com_types
            com_impl_tracing
            com_impl_runtime_mock
            com_message_passing
            score::score_mw
            score::score_filesystem
            score::score_filesystem_i_standard
            score::score_memory_split_string_view
            score::score_mw_log_detail
            score::score_mw_log_configuration
            score::score_mw_log_detail
            score::score_mw_log_configuration
            score::score_mw_log_core
            GTest::gtest_main
    )

    add_executable(com_runtime_configuration_test
        runtime_configuration_test.cpp
    )

    target_link_libraries(com_runtime_configuration_test
        PRIVATE
            com_runtime_configuration
            com_message_passing
            score::score_mw
            score::score_filesystem
            score::score_filesystem_i_standard
            "-Wl,--whole-archive"
            score::score_memory_split_string_view
            "-Wl,--no-whole-archive"
            score::score_mw_log_detail
            score::score_mw_log_configuration
            score::score_mw_log_detail
            score::score_mw_log_configuration
            score::score_mw_log_core
            GTest::gtest_main
    )

    gtest_discover_tests(com_unit_test)
    gtest_discover_tests(com_runtime_configuration_test)
endif()

# Install targets
install(TARGETS
    com_error_domain
    com_types
    com_runtime_configuration
    com_runtime
    com
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
