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

message(STATUS "* SOME/IP Unit Test Dir: ${CMAKE_CURRENT_SOURCE_DIR} ")

file(GLOB_RECURSE SOMEIP_UNIT_TEST_SOURCE CONFIGURE_DEPENDS
    endpoint/*.cpp
    message/*.cpp
    packetrouter/*.cpp
    runtime/ApplicationManagerTest.cpp
    runtime/EventManagerTest.cpp
    runtime/ServiceManagerTest.cpp
    runtime/ServiceManagerPrivateTest.cpp
    exception/ExceptionTest.cpp
    utils/*.cpp
)

add_executable(unittest_someip_unit ${SOMEIP_UNIT_TEST_SOURCE})
someip_configure_target(unittest_someip_unit)
target_link_libraries(unittest_someip_unit
    PRIVATE
        lgsomeip::lgsomeip-shared
        lgsomeip::lgsomeip-daemon-core
        lgsomeip::test-dependencies
        ${ADDITIONAL_LIBS}
)

add_custom_target(unittest_someip
    DEPENDS unittest_someip_unit
)

add_test(NAME someip.unit COMMAND unittest_someip_unit)
set_tests_properties(someip.unit PROPERTIES
    LABELS unit
    TIMEOUT 30
)
