# *******************************************************************************
# Copyright (C) 2017-2026 LG Electronics Inc.
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

##################################################
file(GLOB_RECURSE SOMEIP_TEST_SOURCE CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/someip-response.cpp
)

message(STATUS "* SOMEIP TESTAPP Project base: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "* SOMEIP TESTAPP Sources: ${SOMEIP_TEST_SOURCE}")

set(TEST_NAME "someip-response")

add_executable(${TEST_NAME} ${SOMEIP_TEST_SOURCE})
someip_configure_target(${TEST_NAME})
target_link_libraries(${TEST_NAME} lgsomeip-shared ${ADDITIONAL_LIBS})


##################################################
file(GLOB_RECURSE SOMEIP_TEST_SOURCE CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/someip-request-1.cpp
)

message(STATUS "* SOMEIP TESTAPP Project base: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "* SOMEIP TESTAPP Sources: ${SOMEIP_TEST_SOURCE}")

set(TEST_NAME "someip-request-1")

add_executable(${TEST_NAME} ${SOMEIP_TEST_SOURCE})
someip_configure_target(${TEST_NAME})
target_link_libraries(${TEST_NAME} lgsomeip-shared ${ADDITIONAL_LIBS})

##################################################
file(GLOB_RECURSE SOMEIP_TEST_SOURCE CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/someip-request-2.cpp
)

message(STATUS "* SOMEIP TESTAPP Project base: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "* SOMEIP TESTAPP Sources: ${SOMEIP_TEST_SOURCE}")

set(TEST_NAME "someip-request-2")

add_executable(${TEST_NAME} ${SOMEIP_TEST_SOURCE})
someip_configure_target(${TEST_NAME})
target_link_libraries(${TEST_NAME} lgsomeip-shared ${ADDITIONAL_LIBS})

##################################################
file(GLOB_RECURSE SOMEIP_TEST_SOURCE CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/someip-request-wrong.cpp
)

message(STATUS "* SOMEIP TESTAPP Project base: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "* SOMEIP TESTAPP Sources: ${SOMEIP_TEST_SOURCE}")

set(TEST_NAME "someip-request-wrong")

add_executable(${TEST_NAME} ${SOMEIP_TEST_SOURCE})
someip_configure_target(${TEST_NAME})
target_link_libraries(${TEST_NAME} lgsomeip-shared ${ADDITIONAL_LIBS})
