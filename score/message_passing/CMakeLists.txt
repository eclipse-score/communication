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

# Add subdirectory for non_allocating_future
add_subdirectory(non_allocating_future)

# Common headers library
add_library(message_passing_common_headers
    common_headers.cpp
)

target_include_directories(message_passing_common_headers
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(message_passing_common_headers
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/message_passing
        FILES
            client_server_communication.h
            i_client_connection.h
            i_client_factory.h
            i_connection_handler.h
            i_server_connection.h
            i_server_factory.h
            i_server.h
            i_shared_resource_engine.h
            server_types.h
            service_protocol_config.h
)

target_link_libraries(message_passing_common_headers
    PUBLIC
        score::score_containers
        score::score_language
        score::score_os
)

# Message passing common library
add_library(message_passing_common
    client_connection.cpp
    timed_command_queue.cpp
)

target_include_directories(message_passing_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(message_passing_common
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/message_passing
        FILES
            client_connection.h
            timed_command_queue.h
            timed_command_queue_entry.h
)

target_link_libraries(message_passing_common
    PUBLIC
        message_passing_common_headers
        non_allocating_future
)

# Unix domain implementation
add_library(message_passing_unix_domain
    unix_domain/unix_domain_client_factory.cpp
    unix_domain/unix_domain_engine.cpp
    unix_domain/unix_domain_server.cpp
    unix_domain/unix_domain_server_factory.cpp
)

target_include_directories(message_passing_unix_domain
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(message_passing_unix_domain
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/message_passing
        FILES
            unix_domain/unix_domain_client_factory.h
            unix_domain/unix_domain_engine.h
            unix_domain/unix_domain_server.h
            unix_domain/unix_domain_server_factory.h
            unix_domain/unix_domain_socket_address.h
)

target_link_libraries(message_passing_unix_domain
    PUBLIC
        message_passing_common
        score::score_os
)

add_library(message_passing ALIAS message_passing_unix_domain)

# Add aliases for exported targets
add_library(score-communication::message_passing_common ALIAS message_passing_common)
add_library(score-communication::message_passing_common_headers ALIAS message_passing_common_headers)
add_library(score-communication::message_passing_unix_domain ALIAS message_passing_unix_domain)
add_library(score-communication::com_message_passing ALIAS message_passing_unix_domain)  # Use the actual target

# Mock library for testing
if(SCORE_BUILD_TESTING)
    add_library(message_passing_mock
        mock/mocks.cpp
    )

    target_include_directories(message_passing_mock
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
    )

    target_sources(message_passing_mock
        PUBLIC
            FILE_SET HEADERS
            BASE_DIRS ${CMAKE_SOURCE_DIR}/score/message_passing
            FILES
                mock/client_connection_mock.h
                mock/client_factory_mock.h
                mock/server_connection_mock.h
                mock/server_factory_mock.h
                mock/server_mock.h
                mock/shared_resource_engine_mock.h
    )

    target_link_libraries(message_passing_mock
        PUBLIC
            message_passing_common_headers
            GTest::gmock
    )

    # Unit tests
    add_executable(client_connection_test
        client_connection_test.cpp
    )

    target_link_libraries(client_connection_test
        PRIVATE
            message_passing_common
            message_passing_mock
            GTest::gtest_main
    )

    add_executable(unix_domain_test
        unix_domain_server_test.cpp
        unix_domain_server_to_client_test.cpp
    )

    target_link_libraries(unix_domain_test
        PRIVATE
            message_passing_unix_domain
            GTest::gtest_main
    )

    # Register tests
    gtest_discover_tests(client_connection_test)
    gtest_discover_tests(unix_domain_test)
endif()

# Install targets
install(TARGETS
    message_passing_common_headers
    message_passing_common
    message_passing_unix_domain
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/message_passing
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(SCORE_BUILD_TESTING)
    install(TARGETS message_passing_mock
        EXPORT score-communicationTargets
        FILE_SET HEADERS DESTINATION include/score/message_passing/mock
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()