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

message(STATUS "* OS Abstraction Test Project base: ${CMAKE_CURRENT_SOURCE_DIR} ")

set(OSABSTRACTION_UNIT_TEST_SOURCE
    socket/AddressTest.cpp
    socket/IP4AddressTest.cpp
    socket/IP6AddressTest.cpp
    utils/log/LoggerTest.cpp
    utils/time/TimerManagerTest.cpp
)

add_executable(unittest_osabstraction_unit ${OSABSTRACTION_UNIT_TEST_SOURCE})
lgsomeip_configure_target(unittest_osabstraction_unit)
target_include_directories(unittest_osabstraction_unit PRIVATE
    ${LGSOMEIP_SOURCE_DIR}/src/osabstraction
    ${LGSOMEIP_SOURCE_DIR}/src/someip
)
target_link_libraries(unittest_osabstraction_unit
    PRIVATE
        lgsomeip-shared
        lgsomeip::test-dependencies
        ${ADDITIONAL_LIBS}
)

add_custom_target(unittest_osabstraction
    DEPENDS unittest_osabstraction_unit
)

add_test(NAME osabstraction.unit COMMAND unittest_osabstraction_unit)
set_tests_properties(osabstraction.unit PROPERTIES
    LABELS unit
    TIMEOUT 30
)
