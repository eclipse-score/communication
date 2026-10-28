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

set(OSABSTRACTION_COMPONENT_TEST_SOURCE
    socket/NetworkDeviceTest.cpp
    socket/LocalAddressTest.cpp
    socket/SocketTest.cpp
)

add_executable(unittest_osabstraction_component ${OSABSTRACTION_COMPONENT_TEST_SOURCE})
lgsomeip_configure_target(unittest_osabstraction_component)
target_include_directories(unittest_osabstraction_component PRIVATE
    ${LGSOMEIP_SOURCE_DIR}/src/osabstraction
    ${LGSOMEIP_SOURCE_DIR}/src/someip
)
target_link_libraries(unittest_osabstraction_component
    PRIVATE
        lgsomeip-shared
        lgsomeip::test-dependencies
        ${ADDITIONAL_LIBS}
)

add_test(NAME osabstraction.component COMMAND unittest_osabstraction_component)
set_tests_properties(osabstraction.component PROPERTIES
    LABELS component
    TIMEOUT 30
)
