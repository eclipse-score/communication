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

# Non-allocating future library
add_library(non_allocating_future
    non_allocating_future.cpp
)

target_include_directories(non_allocating_future
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(non_allocating_future
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/message_passing/non_allocating_future
        FILES
            non_allocating_future.h
)

# Tests
if(SCORE_BUILD_TESTING)
    add_executable(non_allocating_future_test
        non_allocating_future_test_samples.cpp
        non_allocating_future_test_unit.cpp
    )

    target_link_libraries(non_allocating_future_test
        PRIVATE
            non_allocating_future
            GTest::gmock
            GTest::gtest_main
    )

    gtest_discover_tests(non_allocating_future_test)
endif()

# Add alias for exported target
add_library(score-communication::non_allocating_future ALIAS non_allocating_future)

# Install targets
install(TARGETS non_allocating_future
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/message_passing/non_allocating_future
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
