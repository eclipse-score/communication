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

file(GLOB_RECURSE SOMEIP_COMPONENT_TEST_SOURCE CONFIGURE_DEPENDS
    config/*.cpp
)

add_executable(unittest_someip_component ${SOMEIP_COMPONENT_TEST_SOURCE})
someip_configure_target(unittest_someip_component)
target_compile_definitions(unittest_someip_component PRIVATE
    LGSOMEIP_TEST_CONFIG_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(unittest_someip_component
    PRIVATE
        lgsomeip::lgsomeip-shared
        lgsomeip::test-dependencies
        ${ADDITIONAL_LIBS}
)

add_test(NAME someip.component COMMAND unittest_someip_component)
set_tests_properties(someip.component PROPERTIES
    LABELS component
    TIMEOUT 30
)
